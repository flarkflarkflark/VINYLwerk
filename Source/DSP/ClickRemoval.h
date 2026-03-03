#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>

class ClickRemoval
{
public:
    ClickRemoval() : sensitivity(60.0f), clickWidth(40) {}

    void prepare(const juce::dsp::ProcessSpec& spec) { sampleRate = spec.sampleRate; }
    void setSensitivity(float s) { sensitivity = juce::jlimit(0.0f, 100.0f, s); }
    void setClickWidth(int w) { clickWidth = juce::jlimit(1, 500, w); }

    const std::vector<double>& getDetectedClickPositions() const { return detectedClickPositions; }

    void process(const juce::dsp::ProcessContextReplacing<float>& context)
    {
        detectedClickPositions.clear();
        auto& block = context.getOutputBlock();
        int numChannels = (int)block.getNumChannels();
        int numSamples = (int)block.getNumSamples();
        float threshold = (101.0f - sensitivity) * 0.005f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* samples = block.getChannelPointer(ch);
            for (int i = 5; i < numSamples - clickWidth - 5; ++i)
            {
                float diff = std::abs(samples[i] - samples[i-1]);
                if (diff > threshold)
                {
                    detectedClickPositions.push_back((double)i / sampleRate);
                    // Simple removal based on width
                    for(int j=0; j < clickWidth; ++j) samples[i+j] = samples[i-1];
                    i += clickWidth + 5;
                }
            }
        }
    }

private:
    float sensitivity;
    int clickWidth;
    double sampleRate;
    std::vector<double> detectedClickPositions;
};