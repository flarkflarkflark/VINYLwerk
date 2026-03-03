#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DSP/ClickRemoval.h"
#include <fstream>

using namespace juce;

int main(int argc, char* argv[])
{
    StringArray args; for (int i = 0; i < argc; ++i) args.add (argv[i]);
    if (args.size() < 3) return 1;
    File inputFile (args[1]); File outputFile (args[2]);
    bool detectOnly = args.contains("--detect-only"); String detectFilePath = "";
    float clickSens = 60.0f; float clickWidth = 200.0f; double startTime = 0.0f; double duration = -1.0f;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--click-sens" && i+1 < args.size()) clickSens = args[++i].getFloatValue();
        else if (args[i] == "--click-width" && i+1 < args.size()) clickWidth = args[++i].getFloatValue();
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
    AudioBuffer<float> buffer ((int) reader->numChannels, (int) totalSamples);
    reader->read (&buffer, 0, (int) totalSamples, startSample, true, true);
    dsp::ProcessSpec spec { reader->sampleRate, (uint32)totalSamples, (uint32)buffer.getNumChannels() };
    dsp::AudioBlock<float> block (buffer); dsp::ProcessContextReplacing<float> context (block);
    ClickRemoval cr; cr.prepare(spec); cr.setSensitivity(clickSens); cr.setClickWidth((int)clickWidth);
    cr.setStoreDetectedClicks(true); cr.setApplyRemoval(!detectOnly);
    cr.process(context);
    if (detectOnly && detectFilePath.isNotEmpty()) {
        std::ofstream f(detectFilePath.toStdString());
        if (f.is_open()) {
            f << "CLICKS_FOUND:"; auto clicks = cr.getDetectedClicks();
            for (size_t i = 0; i < clicks.size(); ++i) f << ((double)(startSample + clicks[i].position) / reader->sampleRate) << (i == clicks.size() - 1 ? "" : ",");
            f << "\n";
        }
    } else if (!detectOnly) {
        WavAudioFormat wavFormat; std::unique_ptr<OutputStream> outStream (outputFile.createOutputStream());
        if (outStream) {
            std::unique_ptr<AudioFormatWriter> writer (wavFormat.createWriterFor(outStream.get(), reader->sampleRate, buffer.getNumChannels(), 24, {}, 0));
            if (writer) { outStream.release(); writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()); }
        }
    }
    return 0;
}