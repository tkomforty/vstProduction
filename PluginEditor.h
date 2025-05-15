#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 * NewVerbTk1 Audio Editor
 * Provides a graphical user interface for the spectral effect plugin.
 */
class NewVerbTk1AudioProcessorEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    NewVerbTk1AudioProcessorEditor(NewVerbTk1AudioProcessor&, juce::AudioProcessorValueTreeState&);
    ~NewVerbTk1AudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

    // Update the spectrogram display
    void updateSpectrogramDisplay();

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    NewVerbTk1AudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState& valueTreeState;

    // GUI elements
    juce::Slider wetDrySlider;
    juce::Slider timeSlider;
    juce::Slider densitySlider;
    juce::Slider dampingSlider;
    juce::Slider sizeSlider;
    juce::Slider lowBandSlider;
    juce::Slider midBandSlider;
    juce::Slider highBandSlider;
    juce::ToggleButton freezeButton;

    // Labels for controls
    juce::Label titleLabel;
    juce::Label wetDryLabel;
    juce::Label timeLabel;
    juce::Label densityLabel;
    juce::Label dampingLabel;
    juce::Label sizeLabel;
    juce::Label lowBandLabel;
    juce::Label midBandLabel;
    juce::Label highBandLabel;
    juce::Label freezeLabel;

    // Attachment objects to connect slider/button values to parameters
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wetDryAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> densityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dampingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sizeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowBandAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> midBandAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highBandAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    // Spectrogram display
    class SpectrogramComponent : public juce::Component
    {
    public:
        SpectrogramComponent(NewVerbTk1AudioProcessor& p);

        void paint(juce::Graphics& g) override;
        void update();

    private:
        NewVerbTk1AudioProcessor& processor;
        juce::Image spectrogramImage;
        juce::Colour gradientColours[6];
    };

    SpectrogramComponent spectrogramDisplay;

    // Generic method to setup a rotary slider
    void setupRotarySlider(juce::Slider& slider, juce::Label& label, const juce::String& labelText);

    void timerCallback() override;

    // Custom look and feel for sliders
    class CustomLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        CustomLookAndFeel();

        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
            const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override;
    };

    CustomLookAndFeel customLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewVerbTk1AudioProcessorEditor)
};