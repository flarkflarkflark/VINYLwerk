#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/ClickRemoval.h"
#include "DSP/NoiseReduction.h"
#include "DSP/FilterBank.h"

class VinylwerkProcessor  : public juce::AudioProcessor
{
public:
    VinylwerkProcessor();
    ~VinylwerkProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VINYLwerk v1.43.0"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Visual Feedback helpers
    int getClicksLastBlock() const { return clickRemoval.getClicksDetectedLastBlock(); }
    
    // v1.36.1: Real-time FFT Data for UI
    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    float* getFFTData() { return fftData.data(); }
    bool nextFFTBlockReady = false;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    ClickRemoval clickRemoval;
    NoiseReduction noiseReduction;
    FilterBank filterBank;

    // FFT visualizer components
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize> fifo;
    std::array<float, fftSize * 2> fftData;
    int fifoIndex = 0;
    juce::String clickReportPath;

    void pushNextSampleIntoFifo (float sample) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VinylwerkProcessor)
};
