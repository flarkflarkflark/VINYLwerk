#include "PluginProcessor.h"
#include "PluginEditor.h"

VinylwerkEditor::VinylwerkEditor (VinylwerkProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    auto setupSlider = [this](juce::Slider& s, juce::String name, juce::String paramID, juce::String suffix, juce::String tooltip) {
        addAndMakeVisible(s);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        s.setTextValueSuffix(suffix);
        s.setTooltip(tooltip);
        attachments.push_back(std::make_unique<Attachment>(audioProcessor.apvts, paramID, s));
    };

    auto setupButton = [this](juce::ToggleButton& b, juce::String name, juce::String paramID, juce::String tooltip) {
        addAndMakeVisible(b);
        b.setButtonText(name);
        b.setTooltip(tooltip);
        buttonAttachments.push_back(std::make_unique<ButtonAttachment>(audioProcessor.apvts, paramID, b));
    };

    setupSlider(clickSensSlider, "Sensitivity", "click_sens", "", "Higher values detect smaller clicks.");
    setupSlider(clickWidthSlider, "Max Width", "click_width", " ms", "Maximum duration of a click to be repaired.");
    setupSlider(noiseRedSlider, "Noise Reduction", "noise_red", " dB", "Amount of surface hiss to remove.");
    setupSlider(rumbleFreqSlider, "Rumble Hz", "rumble", " Hz", "High-pass cutoff to remove record player motor noise.");
    setupSlider(humFreqSlider, "Hum Hz", "hum", " Hz", "Notch filter frequency to remove 50/60Hz electricity hum.");

    setupButton(clickToggle, "Click On", "click_on", "Enable/Disable impulse click removal.");
    setupButton(noiseToggle, "Noise On", "noise_on", "Enable/Disable spectral hiss reduction.");
    setupButton(rumbleToggle, "Rumble On", "rumble_on", "Enable/Disable the ultra-steep 48dB/oct rumble filter.");
    setupButton(humToggle, "Hum On", "hum_on", "Enable/Disable the electricity hum notch filter.");
    setupButton(inverseToggle, "LISTEN TO REMOVED (Delta)", "inverse", "HEAR ONLY WHAT IS BEING REMOVED. Use this to ensure no music is lost.");

    addAndMakeVisible(presetComboBox);
    presetComboBox.addItemList({"Manual", "Balanced Repair", "Deep Clean", "Only Clicks"}, 1);
    presetComboBox.setTooltip("Choose a starting point for restoration.");
    presetAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "preset", presetComboBox);

    for(int i=0; i<20; ++i) scopeData[i] = 0.0f;

    setSize (450, 650);
    setResizable (true, true);
    setResizeLimits (350, 500, 1200, 1600);
    getConstrainer()->setFixedAspectRatio (0.7); 
    
    startTimerHz(60); 
}

VinylwerkEditor::~VinylwerkEditor() { stopTimer(); }

void VinylwerkEditor::timerCallback()
{
    if (audioProcessor.getClicksLastBlock() > 0) clickAlpha = 1.0f;
    else clickAlpha *= 0.85f;

    float* fftData = audioProcessor.getFFTData();
    const int numBands = 20;
    const int fftSize = VinylwerkProcessor::fftSize;

    for (int i = 0; i < numBands; ++i)
    {
        float bandSum = 0.0f;
        int startBin = (int)std::pow(2.0f, (float)i * 10.0f / (float)numBands);
        int endBin = (int)std::pow(2.0f, (float)(i + 1) * 10.0f / (float)numBands);
        startBin = juce::jlimit(0, fftSize/2, startBin);
        endBin = juce::jlimit(startBin + 1, fftSize/2, endBin);
        for (int bin = startBin; bin < endBin; ++bin) bandSum += fftData[bin];
        bandSum /= (float)(endBin - startBin);
        float level = juce::Decibels::gainToDecibels(bandSum);
        level = juce::jlimit(0.0f, 1.0f, (level + 60.0f) / 60.0f);
        if (level > scopeData[i]) scopeData[i] = level;
        else scopeData[i] *= 0.94f;
    }
    repaint();
}

void VinylwerkEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour(0xFF151515));
    float scale = (float)getWidth() / 450.0f;

    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(20.0f * scale);
    g.drawText("VINYLwerk v1.43.0", 10 * scale, 10 * scale, 300 * scale, 30 * scale, juce::Justification::left);

    // 1. Click Flash
    auto visualizerArea = juce::Rectangle<int>(20 * scale, 50 * scale, getWidth() - 40 * scale, 60 * scale);
    g.setColour(juce::Colours::black);
    g.fillRect(visualizerArea);
    g.setColour(juce::Colours::red.withAlpha(clickAlpha));
    g.drawRect(visualizerArea.toFloat(), 2.0f * scale);
    if (clickAlpha > 0.01f) {
        g.setFont(16.0f * scale);
        g.drawText("CLICK DETECTED", visualizerArea, juce::Justification::centred);
    }

    // 2. FFT Spectrum Analyzer with Filter Curves
    auto spectrumArea = juce::Rectangle<int>(20 * scale, 120 * scale, getWidth() - 40 * scale, 120 * scale);
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.fillRoundedRectangle(spectrumArea.toFloat(), 4.0f * scale);
    
    float barW = (float)spectrumArea.getWidth() / 20.0f;
    for (int i = 0; i < 20; ++i)
    {
        float h = scopeData[i] * spectrumArea.getHeight();
        float hue = 0.65f - (float)i * 0.03f; 
        g.setColour(juce::Colour::fromHSV(hue, 0.8f, 0.8f, 0.9f));
        g.fillRect((float)spectrumArea.getX() + i * barW + 1, (float)spectrumArea.getBottom() - h, barW - 2, h);
    }

    // 2b. Draw Filter Curves Overlay
    g.setColour(juce::Colours::cyan.withAlpha(0.5f));
    if (rumbleToggle.getToggleState())
    {
        float rumblePos = (rumbleFreqSlider.getValue() / 150.0f) * (spectrumArea.getWidth() * 0.3f); // Approx bass area
        g.drawVerticalLine((int)(spectrumArea.getX() + rumblePos), (float)spectrumArea.getY(), (float)spectrumArea.getBottom());
        g.setColour(juce::Colours::cyan.withAlpha(0.2f));
        g.fillRect(spectrumArea.getX(), spectrumArea.getY(), (int)rumblePos, spectrumArea.getHeight());
    }

    if (humToggle.getToggleState())
    {
        float humPos = ((humFreqSlider.getValue() - 40.0f) / 40.0f) * (spectrumArea.getWidth() * 0.2f) + (spectrumArea.getWidth() * 0.1f);
        g.setColour(juce::Colours::yellow);
        g.drawVerticalLine((int)(spectrumArea.getX() + humPos), (float)spectrumArea.getY() + 20, (float)spectrumArea.getBottom());
    }

    if (noiseToggle.getToggleState())
    {
        float noiseFloor = (float)noiseRedSlider.getValue() / 24.0f;
        g.setColour(juce::Colours::plum.withAlpha(0.3f));
        int h = (int)(noiseFloor * spectrumArea.getHeight() * 0.5f);
        g.fillRect(spectrumArea.getX(), spectrumArea.getBottom() - h, spectrumArea.getWidth(), h);
    }


    // 3. Labels
    g.setColour(juce::Colours::grey);
    g.setFont(14.0f * scale);
    int y = 280 * scale;
    auto drawLabel = [&](juce::String text) {
        g.drawText(text, 20 * scale, y, 200 * scale, 20 * scale, juce::Justification::left);
        y += 65 * scale;
    };
    drawLabel("Click Removal");
    drawLabel("Noise Reduction");
    drawLabel("Rumble Filter");
    drawLabel("Hum Filter");

    if (inverseToggle.getToggleState()) {
        g.setColour(juce::Colours::orange.withAlpha(0.3f));
        g.fillAll();
        g.setColour(juce::Colours::orange);
        g.drawRect(getLocalBounds().toFloat(), 4.0f * scale);
        g.setFont(juce::Font(18.0f * scale, juce::Font::bold));
        g.drawText("DELTA MONITORING ACTIVE", 0, getHeight() - 85 * scale, getWidth(), 30 * scale, juce::Justification::centred);
    }
}

void VinylwerkEditor::resized()
{
    float scale = (float)getWidth() / 450.0f;
    presetComboBox.setBounds(20 * scale, 250 * scale, getWidth() - 40 * scale, 25 * scale);

    int y = 300 * scale;
    auto setupPos = [&](juce::ToggleButton& b, juce::Slider& s) {
        b.setBounds(20 * scale, y, 100 * scale, 20 * scale);
        s.setBounds(130 * scale, y, getWidth() - 150 * scale, 30 * scale);
        y += 65 * scale;
    };

    setupPos(clickToggle, clickSensSlider);
    clickWidthSlider.setBounds(130 * scale, y - 35 * scale, getWidth() - 150 * scale, 20 * scale);
    setupPos(noiseToggle, noiseRedSlider);
    setupPos(rumbleToggle, rumbleFreqSlider);
    setupPos(humToggle, humFreqSlider);

    inverseToggle.setBounds(20 * scale, getHeight() - 50 * scale, getWidth() - 40 * scale, 30 * scale);
}
