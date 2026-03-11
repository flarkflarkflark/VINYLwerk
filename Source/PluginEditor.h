#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class VinylwerkProcessor;

class VinylwerkEditor  : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    VinylwerkEditor (VinylwerkProcessor&);
    ~VinylwerkEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    VinylwerkProcessor& audioProcessor;
    juce::TooltipWindow tooltipWindow{this}; // Enable tooltips

    // UI Elements
    juce::Slider clickSensSlider, clickWidthSlider, noiseRedSlider, rumbleFreqSlider, humFreqSlider;
    juce::ToggleButton clickToggle, noiseToggle, rumbleToggle, humToggle, inverseToggle;
    juce::ComboBox presetComboBox;
    
    // Attachments
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::vector<std::unique_ptr<Attachment>> attachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::unique_ptr<ComboBoxAttachment> presetAttachment;

    float clickAlpha = 0.0f; // For detection flash
    float scopeData[20]; // 20 bands for the visualizer

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VinylwerkEditor)
};
