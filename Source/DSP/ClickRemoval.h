#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <atomic>

class ClickRemoval
{
public:
    struct ClickInfo { int64_t position; int width; float magnitude; };

    ClickRemoval() = default;

    void prepare (const juce::dsp::ProcessSpec& spec) { sampleRate = spec.sampleRate; reset(); }
    void reset() { detectedClicks.clear(); }

    void setSensitivity (float s) { sensitivity = s; }
    void setClickWidth (int w) { maxClickWidth = w; }
    void setStoreDetectedClicks (bool s) { storeDetectedClicks = s; }
    void setApplyRemoval (bool a) { applyRemoval = a; }
    
    const std::vector<ClickInfo>& getDetectedClicks() const { return detectedClicks; }

    void process (juce::dsp::ProcessContextReplacing<float>& context) {
        auto& block = context.getOutputBlock();
        int numChannels = (int)block.getNumChannels();
        int numSamples = (int)block.getNumSamples();
        float globalRms = 0.001f;
        for (int ch = 0; ch < numChannels; ++ch) {
            float* data = block.getChannelPointer(ch);
            float sum = 0.0f; for(int i=0; i<numSamples; ++i) sum += data[i]*data[i];
            globalRms += std::sqrt(sum/(float)numSamples);
        }
        globalRms /= (float)numChannels;
        float threshold = (0.12f * std::pow(1.0f - sensitivity/100.0f, 2.0f)) * (globalRms * 1.5f + 0.005f);
        for (int i = 16; i < numSamples - 100; ++i) {
            bool clickDetected = false;
            for (int ch = 0; ch < numChannels; ++ch) {
                if (std::abs(block.getChannelPointer(ch)[i] - 2.0f * block.getChannelPointer(ch)[i-1] + block.getChannelPointer(ch)[i-2]) > threshold) { clickDetected = true; break; }
            }
            if (clickDetected) {
                int width = 1;
                while (i + width < numSamples - 10 && width < maxClickWidth) {
                    bool still = false; for (int ch = 0; ch < numChannels; ++ch) if (std::abs(block.getChannelPointer(ch)[i+width] - block.getChannelPointer(ch)[i+width-1]) > threshold * 0.05f) still = true;
                    if (!still) break; width++;
                }
                if (storeDetectedClicks) detectedClicks.push_back({(int64_t)i, width, threshold});
                if (applyRemoval) {
                    for (int ch = 0; ch < numChannels; ++ch) {
                        float* data = block.getChannelPointer(ch);
                        int s = std::max(8, i - 4), e = std::min(numSamples - 16, i + width + 4);
                        float p0 = data[s-8], p1 = data[s], p2 = data[e], p3 = data[e+8];
                        for (int j = s; j <= e; ++j) {
                            float t = (float)(j - s) / (float)(std::max(1, e - s));
                            float t2 = t * t, t3 = t2 * t;
                            data[j] = 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
                        }
                    }
                }
                i += width + 2; 
            }
        }
    }
private:
    double sampleRate = 44100.0; float sensitivity = 50.0f; int maxClickWidth = 200; bool storeDetectedClicks = false; bool applyRemoval = true; std::vector<ClickInfo> detectedClicks;
};