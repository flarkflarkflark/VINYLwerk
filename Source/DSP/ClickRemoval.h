#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <atomic>

class ClickRemoval
{
public:
    struct ClickInfo { int64_t position; int width; float magnitude; bool isManual = false; };

    enum RemovalMethod { SplineInterpolation, CrossfadeSmoothing, Automatic };

    ClickRemoval() = default;

    void prepare (const juce::dsp::ProcessSpec& spec) { sampleRate = spec.sampleRate; reset(); }

    void reset() { detectedClicks.clear(); currentSamplePosition = 0; }

    void setSensitivity (float s)          { sensitivity = juce::jlimit (0.0f, 100.0f, s); }
    void setClickWidth (int w)             { maxClickWidth = w; }   // legacy alias
    void setMaxWidth (int w)               { maxClickWidth = w; }
    void setStoreDetectedClicks (bool s)   { storeDetectedClicks = s; }
    void setApplyRemoval (bool a)          { applyRemoval = a; }
    void setRemovalMethod (RemovalMethod m){ removalMethod = m; }
    void resetSamplePosition()             { currentSamplePosition = 0; }
    void setSampleOffset (int64_t offset)  { currentSamplePosition = offset; }

    const std::vector<ClickInfo>& getDetectedClicks() const { return detectedClicks; }
    int   getClicksDetectedLastBlock() const { return clicksDetectedLastBlock.load(); }
    float getClickRate()               const { return clickRatePerSecond.load(); }

    void addManualClick (int64_t samplePosition, int width)
    {
        ClickInfo click;
        click.position = samplePosition;
        click.width    = width;
        click.isManual = true;
        detectedClicks.push_back (click);
    }

    void process (juce::dsp::ProcessContextReplacing<float>& context)
    {
        auto& block = context.getOutputBlock();
        int clicksThisBlock = 0;

        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* data       = block.getChannelPointer (ch);
            auto  numSamples = block.getNumSamples();

            if (numSamples < 10)
                continue;

            float rmsLevel        = calculateRMS (data, numSamples);
            float adaptThreshold  = calculateThreshold (rmsLevel);

            for (size_t i = 4; i < numSamples - 4; ++i)
            {
                float secondDeriv = std::abs (data[i] - 2.0f * data[i - 1] + data[i - 2]);

                if (secondDeriv > adaptThreshold)
                {
                    // Peak-finding lookahead: refine to the sharpest sample within ±5
                    size_t peakPos  = i;
                    float  maxDeriv = secondDeriv;
                    for (size_t j = i + 1; j < juce::jmin (i + 5, numSamples - 4); ++j)
                    {
                        float d = std::abs (data[j] - 2.0f * data[j - 1] + data[j - 2]);
                        if (d > maxDeriv) { maxDeriv = d; peakPos = j; }
                    }
                    i = peakPos;

                    // Periodicity check — avoid false positives on musical transients
                    if (!isPeriodic (data, (int) i, numSamples, rmsLevel))
                    {
                        int clickWidth = estimateClickWidth (data, (int) i, numSamples);

                        if (clickWidth > 0 && clickWidth <= maxClickWidth)
                        {
                            if (storeDetectedClicks)
                            {
                                ClickInfo info;
                                info.position  = currentSamplePosition + (int64_t) i;
                                info.width     = clickWidth;
                                info.magnitude = secondDeriv / adaptThreshold;
                                info.isManual  = false;
                                detectedClicks.push_back (info);
                            }

                            if (applyRemoval)
                                removeClickAt (data, (int) i, clickWidth, numSamples);

                            ++clicksThisBlock;
                            i += (size_t) clickWidth;
                        }
                    }
                }
            }
        }

        currentSamplePosition += (int64_t) block.getNumSamples();
        clicksDetectedLastBlock.store (clicksThisBlock);

        if (sampleRate > 0 && block.getNumSamples() > 0)
        {
            float blockDuration = (float) block.getNumSamples() / (float) sampleRate;
            clickRatePerSecond.store (blockDuration > 0.0f ? (float) clicksThisBlock / blockDuration : 0.0f);
        }
    }

private:
    //==============================================================================
    float calculateRMS (const float* data, size_t numSamples)
    {
        float sum = 0.0f;
        for (size_t i = 0; i < numSamples; ++i)
            sum += data[i] * data[i];
        return std::sqrt (sum / (float) numSamples);
    }

    float calculateThreshold (float rmsLevel)
    {
        // Non-linear sensitivity curve over a data-calibrated range.
        // maxThreshold: conservative (only large pops).
        // minThreshold: aggressive (subtle ticks at maximum sensitivity).
        const float maxThreshold = 0.350f;
        const float minThreshold = 0.0002f;

        float normalizedSensitivity = juce::jlimit (0.0f, 1.0f, sensitivity / 100.0f);
        float curve         = std::pow (1.0f - normalizedSensitivity, 2.0f);
        float baseThreshold = minThreshold + (maxThreshold - minThreshold) * curve;

        // Adaptive level factor — more sensitive in quiet passages.
        // Clamped to prevent threshold blow-up in loud material.
        float levelFactor = juce::jlimit (0.02f, 1.5f, rmsLevel * 1.5f + 0.05f);

        return baseThreshold * levelFactor;
    }

    // Returns true when the candidate position looks like periodic (musical) content
    // rather than a genuine click, to reduce false positives.
    bool isPeriodic (const float* data, int position, size_t numSamples, float rmsLevel)
    {
        const int checkRange = 24;
        if (position < checkRange || position + checkRange >= (int) numSamples)
            return false;

        float mad = 0.0f;
        for (int offset = 1; offset <= checkRange; ++offset)
            mad += std::abs (data[position] - data[position + offset]);
        mad /= (float) checkRange;

        // At high sensitivity, be more lenient about what counts as periodic
        float sensitivityFactor = juce::jlimit (0.05f, 1.0f, 1.0f - (sensitivity / 105.0f));
        float periodicThreshold = (rmsLevel * 0.3f + 0.005f) * sensitivityFactor;

        return mad < periodicThreshold;
    }

    // Bidirectional width estimation from the transient peak.
    // Uses 15% of the local peak magnitude as the boundary criterion.
    int estimateClickWidth (const float* data, int position, size_t numSamples)
    {
        int maxSearch = juce::jmin (maxClickWidth / 2, 64);
        int start     = juce::jmax (0, position - maxSearch);
        int end       = juce::jmin ((int) numSamples - 1, position + maxSearch);

        float centerValue = data[position];

        float peakMag = 0.0f;
        for (int i = start; i <= end; ++i)
            peakMag = juce::jmax (peakMag, std::abs (data[i] - centerValue));

        float widthThreshold = juce::jmax (0.0001f, peakMag * 0.15f);

        int widthBefore = 0;
        for (int i = position - 1; i >= start; --i)
        {
            if (std::abs (data[i] - centerValue) < widthThreshold) break;
            widthBefore++;
        }

        int widthAfter = 0;
        for (int i = position + 1; i <= end; ++i)
        {
            if (std::abs (data[i] - centerValue) < widthThreshold) break;
            widthAfter++;
        }

        return juce::jmax (1, widthBefore + widthAfter + 1);
    }

    //==============================================================================
    void removeClickAt (float* data, int position, int clickWidth, size_t numSamples)
    {
        RemovalMethod method = removalMethod;
        if (method == Automatic)
        {
            // Short pops (<50 samples, ~1.1 ms @ 44.1 kHz): cosine crossfade.
            // Longer clicks: cubic spline for higher quality reconstruction.
            method = (clickWidth <= 50) ? CrossfadeSmoothing : SplineInterpolation;
        }

        if (method == CrossfadeSmoothing)
            applyCrossfadeSmoothing (data, position, clickWidth, numSamples);
        else
            applyCubicSpline (data, position, clickWidth, numSamples);
    }

    // Cosine-curve crossfade — fast and transparent for short pops.
    void applyCrossfadeSmoothing (float* data, int position, int clickWidth, size_t numSamples)
    {
        int fadeLength = juce::jmax (clickWidth / 2, 16);
        int fadeStart  = juce::jmax (0, position - fadeLength);
        int fadeEnd    = juce::jmin ((int) numSamples - 1, position + fadeLength);

        if (fadeEnd <= fadeStart + 1)
            return;

        float startValue = data[fadeStart];
        float endValue   = data[fadeEnd];
        int   fadeRange  = fadeEnd - fadeStart;

        for (int i = fadeStart; i <= fadeEnd; ++i)
        {
            float phase  = (float) (i - fadeStart) / (float) fadeRange;
            float weight = 0.5f - 0.5f * std::cos (phase * juce::MathConstants<float>::pi);
            data[i] = startValue + (endValue - startValue) * weight;
        }
    }

    // Catmull-Rom cubic spline with averaged control points for stability.
    void applyCubicSpline (float* data, int position, int clickWidth, size_t numSamples)
    {
        int interpolateLength = juce::jmax (clickWidth, 8);
        int start = juce::jmax (0, position - interpolateLength / 2);
        int end   = juce::jmin ((int) numSamples - 1, position + interpolateLength / 2);

        if (end <= start + 1)
            return;

        const int margin = 4;
        int p0Idx = juce::jmax (0, start - margin);
        int p3Idx = juce::jmin ((int) numSamples - 1, end + margin);

        float p0 = averageSamples (data, p0Idx,    juce::jmin (margin, start - p0Idx));
        float p1 = data[start];
        float p2 = data[end];
        float p3 = averageSamples (data, end + 1,  juce::jmin (margin, p3Idx - end));

        int range = end - start;
        for (int i = start; i <= end; ++i)
        {
            float t  = (float) (i - start) / (float) range;
            data[i] = catmullRomInterpolate (p0, p1, p2, p3, t);
        }
    }

    float catmullRomInterpolate (float p0, float p1, float p2, float p3, float t)
    {
        float t2 = t * t;
        float t3 = t2 * t;
        float v0 = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
        float v1 =  p0    - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
        float v2 = -0.5f * p0 + 0.5f * p2;
        float v3 =  p1;
        return v0 * t3 + v1 * t2 + v2 * t + v3;
    }

    float averageSamples (const float* data, int startIdx, int count)
    {
        if (count <= 0)
            return data[juce::jmax (0, startIdx)];
        float sum = 0.0f;
        for (int i = 0; i < count; ++i)
            sum += data[startIdx + i];
        return sum / (float) count;
    }

    //==============================================================================
    double sampleRate           = 44100.0;
    float  sensitivity          = 50.0f;
    int    maxClickWidth        = 200;
    RemovalMethod removalMethod = Automatic;

    bool    storeDetectedClicks  = false;
    bool    applyRemoval         = true;
    int64_t currentSamplePosition = 0;

    std::vector<ClickInfo>     detectedClicks;
    std::atomic<int>           clicksDetectedLastBlock { 0 };
    std::atomic<float>         clickRatePerSecond      { 0.0f };
};
