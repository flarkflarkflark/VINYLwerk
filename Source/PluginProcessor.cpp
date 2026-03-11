#include "PluginProcessor.h"
#include "PluginEditor.h"

VinylwerkProcessor::VinylwerkProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      forwardFFT (fftOrder),
      window (fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    fifo.fill(0.0f);
    fftData.fill(0.0f);

    // v1.43: Determine click report path (shared file for Lua sync)
#if JUCE_WINDOWS
    clickReportPath = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("flark_vinyl_clicks.txt").getFullPathName();
#else
    clickReportPath = "/tmp/flark_vinyl_clicks.txt";
#endif
}

VinylwerkProcessor::~VinylwerkProcessor() {}

void VinylwerkProcessor::pushNextSampleIntoFifo (float sample) noexcept
{
    if (fifoIndex == fftSize)
    {
        if (! nextFFTBlockReady)
        {
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            std::copy (fifo.begin(), fifo.end(), fftData.begin());
            nextFFTBlockReady = true;
        }
        fifoIndex = 0;
    }
    fifo[fifoIndex++] = sample;
}

juce::AudioProcessorValueTreeState::ParameterLayout VinylwerkProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Balanced Defaults: Sens 45, Width 3.0, Noise 10, Rumble 25
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("click_sens", "Click Sensitivity", 0.0f, 100.0f, 45.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("click_width", "Max Click Width", 0.1f, 500.0f, 3.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("noise_red", "Noise Reduction", 0.0f, 24.0f, 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("rumble", "Rumble Filter", 5.0f, 150.0f, 25.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hum", "Hum Filter", 40.0f, 80.0f, 50.0f));
    
    params.push_back (std::make_unique<juce::AudioParameterBool> ("click_on", "Click Removal On", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("noise_on", "Noise Reduction On", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("rumble_on", "Rumble Filter On", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("hum_on", "Hum Filter On", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("inverse", "Listen to Removed (Delta)", false));
    params.push_back (std::make_unique<juce::AudioParameterInt> ("click_count", "Internal Click Count", 0, 1000000, 0));
    
    juce::StringArray presetNames = { "Manual", "Balanced Repair", "Deep Clean", "Only Clicks" };
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("preset", "Restoration Preset", presetNames, 1));

    return { params.begin(), params.end() };
}

void VinylwerkProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) getTotalNumOutputChannels() };
    clickRemoval.prepare (spec);
    noiseReduction.prepare (spec);
    filterBank.prepare (spec);
}

void VinylwerkProcessor::releaseResources() {}

void VinylwerkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto numChannels = buffer.getNumChannels();
    auto numSamples = buffer.getNumSamples();

    // ── 1. Handle Presets ───────────────────────────────────────────────
    int presetIdx = (int)*apvts.getRawParameterValue ("preset");
    if (presetIdx > 0)
    {
        auto set = [&](juce::String id, float val) {
            apvts.getParameter(id)->setValueNotifyingHost(
                apvts.getParameter(id)->getNormalisableRange().convertTo0to1(val));
        };
        auto setB = [&](juce::String id, bool val) {
            apvts.getParameter(id)->setValueNotifyingHost(val ? 1.0f : 0.0f);
        };

        if (presetIdx == 1) { // Balanced Repair
            set("click_sens", 45.0f); set("click_width", 3.0f); set("noise_red", 10.0f); set("rumble", 25.0f);
            setB("click_on", true); setB("noise_on", true); setB("rumble_on", true); setB("hum_on", false);
        } else if (presetIdx == 2) { // Deep Clean
            set("click_sens", 65.0f); set("click_width", 5.0f); set("noise_red", 16.0f); set("rumble", 35.0f);
            setB("click_on", true); setB("noise_on", true); setB("rumble_on", true); setB("hum_on", true);
        } else if (presetIdx == 3) { // Only Clicks
            set("click_sens", 40.0f); set("click_width", 2.5f); set("noise_red", 0.0f); set("rumble", 20.0f);
            setB("click_on", true); setB("noise_on", false); setB("rumble_on", false); setB("hum_on", false);
        }
        apvts.getParameter("preset")->setValueNotifyingHost(0.0f);
    }

    // ── 2. Store original for Delta mode ────────────────────────────────
    juce::AudioBuffer<float> originalBuffer;
    bool isInverse = *apvts.getRawParameterValue ("inverse");
    if (isInverse)
        originalBuffer.makeCopyOf (buffer);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // ── 3. Set click removal parameters ─────────────────────────────────
    clickRemoval.setSensitivity (*apvts.getRawParameterValue ("click_sens"));
    
    float widthMs = *apvts.getRawParameterValue ("click_width");
    int widthSamples = (int)(widthMs * getSampleRate() / 1000.0);
    clickRemoval.setClickWidth (widthSamples);
    clickRemoval.setApplyRemoval (*apvts.getRawParameterValue ("click_on"));

    // v1.43: Sample-accurate position from DAW timeline
    auto* playHead = getPlayHead();
    if (playHead != nullptr)
    {
        auto posInfo = playHead->getPosition();
        if (posInfo.hasValue())
        {
            auto timeInSamples = posInfo->getTimeInSamples();
            if (timeInSamples.hasValue())
                clickRemoval.setSampleOffset (*timeInSamples);
        }
    }

    // ── 4. DSP Chain ────────────────────────────────────────────────────
    noiseReduction.setReduction (*apvts.getRawParameterValue ("noise_red"));
    filterBank.setRumbleFilter (*apvts.getRawParameterValue ("rumble"), !*apvts.getRawParameterValue ("rumble_on"));
    filterBank.setHumFilter (*apvts.getRawParameterValue ("hum"), !*apvts.getRawParameterValue ("hum_on"));

    if (*apvts.getRawParameterValue ("click_on")) 
    {
        clickRemoval.process (context);
        int clicks = clickRemoval.getClicksDetectedLastBlock();
        if (clicks > 0)
        {
            // v1.43: Bump the click_count parameter (signal to Lua)
            auto* param = apvts.getParameter ("click_count");
            float next = param->getValue() + 0.01f;
            if (next > 1.0f) next = 0.0f;
            param->setValueNotifyingHost (next);
        }
    }

    if (*apvts.getRawParameterValue ("rumble_on") || *apvts.getRawParameterValue ("hum_on")) 
        filterBank.process (context, isInverse);

    // ── 5. Delta Mode ───────────────────────────────────────────────────
    if (isInverse)
    {
        bool rumbleActive = *apvts.getRawParameterValue ("rumble_on");
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* out = buffer.getWritePointer (ch);
            auto* orig = originalBuffer.getReadPointer (ch);
            for (int s = 0; s < numSamples; ++s)
            {
                if (! rumbleActive)
                    out[s] = orig[s] - out[s];
            }
        }
    }

    // ── 6. FFT Analyzer ─────────────────────────────────────────────────
    auto* channelData = buffer.getReadPointer(0);
    for (int i = 0; i < numSamples; ++i)
        pushNextSampleIntoFifo (channelData[i]);

    if (nextFFTBlockReady)
    {
        window.multiplyWithWindowingTable (fftData.data(), fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform (fftData.data());
        nextFFTBlockReady = false;
    }
}

juce::AudioProcessorEditor* VinylwerkProcessor::createEditor() { return new VinylwerkEditor (*this); }

void VinylwerkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void VinylwerkProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VinylwerkProcessor(); }
