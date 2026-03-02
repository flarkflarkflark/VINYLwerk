#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DSP/ClickRemoval.h"
#include "DSP/NoiseReduction.h"
#include "DSP/FilterBank.h"

using namespace juce;

int main(int argc, char* argv[])
{
    StringArray args = StringArray::fromTokens(argv, argc);
    
    if (args.size() < 3)
    {
        std::cout << "Usage: vinylwerk_cli <input.wav> <output.wav> [options]
";
        std::cout << "Options:
";
        std::cout << "  --click-sens <0-100>
";
        std::cout << "  --noise-red <dB>
";
        std::cout << "  --rumble <Hz>
";
        std::cout << "  --hum <Hz>
";
        return 1;
    }

    File inputFile(args[1]);
    File outputFile(args[2]);

    if (!inputFile.existsAsFile())
    {
        std::cerr << "Input file does not exist.
";
        return 1;
    }

    float clickSens = 0.0f;
    float noiseRed = 0.0f;
    float rumbleFreq = 0.0f;
    float humFreq = 0.0f;

    for (int i = 3; i < args.size(); ++i)
    {
        if (args[i] == "--click-sens" && i + 1 < args.size()) clickSens = args[++i].getFloatValue();
        else if (args[i] == "--noise-red" && i + 1 < args.size()) noiseRed = args[++i].getFloatValue();
        else if (args[i] == "--rumble" && i + 1 < args.size()) rumbleFreq = args[++i].getFloatValue();
        else if (args[i] == "--hum" && i + 1 < args.size()) humFreq = args[++i].getFloatValue();
    }

    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(inputFile));
    if (reader == nullptr)
    {
        std::cerr << "Failed to read input audio file.
";
        return 1;
    }

    AudioBuffer<float> buffer((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&buffer, 0, (int)reader->lengthInSamples, 0, true, true);

    double sampleRate = reader->sampleRate;

    // Process
    dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (uint32)buffer.getNumSamples();
    spec.numChannels = (uint32)buffer.getNumChannels();

    dsp::AudioBlock<float> block(buffer);
    dsp::ProcessContextReplacing<float> context(block);

    if (clickSens > 0.0f)
    {
        ClickRemoval clickRemoval;
        clickRemoval.prepare(spec);
        clickRemoval.setSensitivity(clickSens);
        clickRemoval.process(context);
    }

    if (noiseRed > 0.0f)
    {
        NoiseReduction noiseReduction;
        noiseReduction.prepare(spec);
        noiseReduction.setReduction(noiseRed);
        noiseReduction.process(context);
    }

    if (rumbleFreq > 0.0f || humFreq > 0.0f)
    {
        FilterBank filters;
        filters.prepare(spec);
        if (rumbleFreq > 0.0f) filters.setRumbleFilter(rumbleFreq, false);
        if (humFreq > 0.0f) filters.setHumFilter(humFreq, false);
        filters.process(context);
    }

    // Write output
    WavAudioFormat wavFormat;
    std::unique_ptr<OutputStream> outStream(outputFile.createOutputStream());
    if (outStream == nullptr)
    {
        std::cerr << "Failed to create output file.
";
        return 1;
    }

    std::unique_ptr<AudioFormatWriter> writer(wavFormat.createWriterFor(
        outStream.get(), sampleRate, buffer.getNumChannels(), 24, {}, 0));
    
    if (writer != nullptr)
    {
        outStream.release(); // Writer takes ownership
        writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    }

    std::cout << "Processing complete.
";
    return 0;
}
