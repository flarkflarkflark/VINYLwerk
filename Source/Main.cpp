#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DSP/ClickRemoval.h"
#include "DSP/NoiseReduction.h"
#include "DSP/FilterBank.h"
#include <fstream>

using namespace juce;

int main(int argc, char* argv[])
{
    StringArray args;
    for (int i = 0; i < argc; ++i) args.add (argv[i]);
    if (args.size() < 3) return 1;

    File inputFile (args[1]);
    File outputFile (args[2]);
    bool detectOnly = args.contains("--detect-only");
    String detectFilePath = "";
    
    float clickSens = 60.0f; float clickWidth = 200.0f; float noiseRed = 0.0f;
    float rumbleFreq = 0.0f; float humFreq = 0.0f; double startTime = 0.0f; double duration = -1.0f;

    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--click-sens" && i+1 < args.size()) clickSens = args[++i].getFloatValue();
        else if (args[i] == "--click-width" && i+1 < args.size()) clickWidth = args[++i].getFloatValue();
        else if (args[i] == "--noise-red" && i+1 < args.size()) noiseRed = args[++i].getFloatValue();
        else if (args[i] == "--rumble" && i+1 < args.size()) rumbleFreq = args[++i].getFloatValue();
        else if (args[i] == "--hum" && i+1 < args.size()) humFreq = args[++i].getFloatValue();
        else if (args[i] == "--start" && i+1 < args.size()) startTime = args[++i].getDoubleValue();
        else if (args[i] == "--duration" && i+1 < args.size()) duration = args[++i].getDoubleValue();
        else if (args[i] == "--detect-file" && i+1 < args.size()) detectFilePath = args[++i];
    }

    AudioFormatManager formatManager; formatManager.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (inputFile));
    if (!reader) return 1;

    int64 startSample = (int64)(startTime * reader->sampleRate);
    int64 totalSamples = (duration < 0) ? (reader->lengthInSamples - startSample) : (int64)(duration * reader->sampleRate);
    totalSamples = jlimit((int64)0, reader->lengthInSamples - startSample, totalSamples);

    if (totalSamples <= 0) return 1;

    AudioBuffer<float> buffer ((int) reader->numChannels, (int) totalSamples);
    reader->read (&buffer, 0, (int) totalSamples, startSample, true, true);

    dsp::ProcessSpec spec { reader->sampleRate, (uint32)totalSamples, (uint32)buffer.getNumChannels() };
    dsp::AudioBlock<float> block (buffer);
    dsp::ProcessContextReplacing<float> context (block);

    ClickRemoval cr;
    cr.prepare(spec);
    cr.setSensitivity(clickSens);
    cr.setClickWidth((int)clickWidth);
    cr.setStoreDetectedClicks(true);
    cr.setApplyRemoval(!detectOnly);
    cr.setRemovalMethod(ClickRemoval::Automatic);
    
    cr.process(context);

    if (detectOnly && detectFilePath.isNotEmpty()) {
        std::ofstream f(detectFilePath.toStdString());
        if (f.is_open()) {
            f << "CLICKS_FOUND:";
            auto clicks = cr.getDetectedClicks();
            for (size_t i = 0; i < clicks.size(); ++i) {
                f << (clicks[i].position) << (i == clicks.size() - 1 ? "" : ",");
            }
            f << "\n";
            f.close();
        }
    } else if (!detectOnly) {
        if (noiseRed > 0.0f) { NoiseReduction nr; nr.prepare(spec); nr.setReduction(noiseRed); nr.process(context); }
        if (rumbleFreq > 0.0f || humFreq > 0.0f) {
            FilterBank fb; fb.prepare(spec);
            if (rumbleFreq > 0.0f) fb.setRumbleFilter(rumbleFreq, false);
            if (humFreq > 0.0f) fb.setHumFilter(humFreq, false);
            fb.process(context);
        }
        WavAudioFormat wavFormat;
        std::unique_ptr<OutputStream> outStream (outputFile.createOutputStream());
        if (outStream) {
            std::unique_ptr<AudioFormatWriter> writer (wavFormat.createWriterFor(outStream.get(), reader->sampleRate, buffer.getNumChannels(), 24, {}, 0));
            if (writer) { outStream.release(); writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()); }
        }
    }
    return 0;
}