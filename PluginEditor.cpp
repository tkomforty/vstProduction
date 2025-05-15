#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Custom Look and Feel implementation
NewVerbTk1AudioProcessorEditor::CustomLookAndFeel::CustomLookAndFeel()
{
    setColour(juce::Slider::thumbColourId, juce::Colour(65, 172, 255));
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(65, 172, 255));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::darkgrey);
}

void NewVerbTk1AudioProcessorEditor::CustomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, const float rotaryStartAngle,
    const float rotaryEndAngle, juce::Slider& slider)
{
    // Calculate useful values
    const float radius = juce::jmin(width / 2, height / 2) - 4.0f;
    const float centerX = x + width * 0.5f;
    const float centerY = y + height * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Draw outline circle
    g.setColour(findColour(juce::Slider::rotarySliderOutlineColourId));
    g.fillEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2);

    // Draw inner circle
    g.setColour(juce::Colours::black);
    g.fillEllipse(centerX - radius * 0.75f, centerY - radius * 0.75f, radius * 1.5f, radius * 1.5f);

    // Draw colored arc
    juce::Path arc;
    arc.addArc(centerX - radius, centerY - radius, radius * 2, radius * 2, rotaryStartAngle, angle, true);
    g.setColour(findColour(juce::Slider::rotarySliderFillColourId));
    g.strokePath(arc, juce::PathStrokeType(3.0f));

    // Draw pointer
    juce::Path pointer;
    pointer.addRectangle(-2.0f, -radius * 0.6f, 4.0f, radius * 0.6f);

    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centerX, centerY));
    g.setColour(findColour(juce::Slider::thumbColourId));
    g.fillPath(pointer);
}

//==============================================================================
// Spectrogram Component Implementation
NewVerbTk1AudioProcessorEditor::SpectrogramComponent::SpectrogramComponent(NewVerbTk1AudioProcessor& p)
    : processor(p)
{
    // Initialize color gradient for spectrogram
    gradientColours[0] = juce::Colours::black;
    gradientColours[1] = juce::Colour(0, 0, 80);
    gradientColours[2] = juce::Colour(0, 0, 160);
    gradientColours[3] = juce::Colour(0, 80, 160);
    gradientColours[4] = juce::Colour(0, 160, 200);
    gradientColours[5] = juce::Colours::white;

    // Create spectrogram image buffer with transparency
    spectrogramImage = juce::Image(juce::Image::RGB, 512, 256, true);
}

void NewVerbTk1AudioProcessorEditor::SpectrogramComponent::paint(juce::Graphics& g)
{
    // Draw background
    g.fillAll(juce::Colours::black);

    // Draw the spectrogram image
    g.drawImage(spectrogramImage, getLocalBounds().toFloat());

    // Draw frequency grid lines and labels
    g.setColour(juce::Colours::darkgrey.withAlpha(0.5f));

    float width = static_cast<float>(getWidth());
    float height = static_cast<float>(getHeight());

    // Draw horizontal grid lines (frequency)
    for (int i = 0; i < 10; ++i)
    {
        float y = height * (1.0f - i / 9.0f);
        g.drawLine(0, y, width, y, 1.0f);

        // Draw frequency labels (logarithmic scale)
        float freq = 20.0f * std::pow(10.0f, 3.0f * i / 9.0f);
        juce::String freqText;

        if (freq < 1000.0f)
            freqText = juce::String(static_cast<int>(freq)) + " Hz";
        else
            freqText = juce::String(freq / 1000.0f, 1) + " kHz";

        g.setColour(juce::Colours::white);
        g.setFont(12.0f);
        g.drawText(freqText, 5, static_cast<int>(y - 12), 60, 20, juce::Justification::left);
    }

    // Add border
    g.setColour(juce::Colours::darkgrey);
    g.drawRect(getLocalBounds(), 1);
}

void NewVerbTk1AudioProcessorEditor::SpectrogramComponent::update()
{
    // Get magnitude data from the processor
    const float* spectralData = processor.getSpectralMagnitudeBuffer();
    const int fftSize = processor.getFFTSize();

    // Create a new image for the updated spectrogram
    juce::Image newImage(juce::Image::RGB, getWidth(), getHeight(), true);
    juce::Graphics g(newImage);

    // Copy existing image, shifted left
    g.drawImage(spectrogramImage, -1, 0, getWidth() - 1, getHeight(),
        1, 0, getWidth() - 1, getHeight());

    // Draw the new column of data
    const int numBins = fftSize / 2;
    const float height = getHeight();

    for (int y = 0; y < height; ++y)
    {
        // Map y coordinate to FFT bin (logarithmic scale)
        float binPosition = std::pow(static_cast<float>(y) / height, 2.5f) * numBins;
        int binIndex = juce::jlimit(0, numBins - 1, static_cast<int>(binPosition));

        // Get magnitude and convert to decibels with some scaling to improve visualization
        float magnitude = spectralData[binIndex];
        float level = juce::jlimit(0.0f, 1.0f, 0.35f * std::log10(1.0f + 100.0f * magnitude));

        // Map level to color using gradient
        juce::Colour color;

        if (level <= 0.0f)
            color = gradientColours[0];
        else if (level >= 1.0f)
            color = gradientColours[5];
        else
        {
            float pos = level * 5.0f;
            int index = static_cast<int>(pos);
            float alpha = pos - index;

            color = gradientColours[index].interpolatedWith(gradientColours[index + 1], alpha);
        }

        // Draw the pixel
        g.setColour(color);
        g.setColour(color);
        g.fillRect(getWidth() - 1, static_cast<int>(height - 1 - y), 1, 1);
    }

    // Update the spectrogram image
    spectrogramImage = newImage;

    // Trigger a repaint
    repaint();
}

//==============================================================================
NewVerbTk1AudioProcessorEditor::NewVerbTk1AudioProcessorEditor(NewVerbTk1AudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor(&p), audioProcessor(p), valueTreeState(vts), spectrogramDisplay(p)
{
    // Set custom look and feel
    setLookAndFeel(&customLookAndFeel);

    // Set up title
    titleLabel.setText("NewVerbTk1 - Spectral Sculptor", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // Set up all the sliders and buttons
    setupRotarySlider(wetDrySlider, wetDryLabel, "Wet/Dry");
    setupRotarySlider(timeSlider, timeLabel, "Time");
    setupRotarySlider(densitySlider, densityLabel, "Density");
    setupRotarySlider(dampingSlider, dampingLabel, "Damping");
    setupRotarySlider(sizeSlider, sizeLabel, "Size");
    setupRotarySlider(lowBandSlider, lowBandLabel, "Low");
    setupRotarySlider(midBandSlider, midBandLabel, "Mid");
    setupRotarySlider(highBandSlider, highBandLabel, "High");

    // Set up freeze button
    freezeButton.setButtonText("Freeze");
    freezeButton.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(freezeButton);

    freezeLabel.setText("Freeze", juce::dontSendNotification);
    freezeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(freezeLabel);

    // Create parameter attachments
    wetDryAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        valueTreeState, "wet_dry", wetDrySlider);
    timeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        valueTreeState, "time", timeSlider);
    densityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        valueTreeState, "density", densitySlider);
    dampingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        valueTreeState, "damping", dampingSlider);
    sizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        valueTreeState, "size", sizeSlider);
    lowBandAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        valueTreeState, "low_band", lowBandSlider);
    midBandAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        valueTreeState, "mid_band", midBandSlider);
    highBandAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        valueTreeState, "high_band", highBandSlider);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        valueTreeState, "freeze", freezeButton);

    // Add spectrogram component
    addAndMakeVisible(spectrogramDisplay);

    // Set initial window size
    setSize(700, 500);

    // Start timer for GUI updates
    startTimerHz(30);
}

NewVerbTk1AudioProcessorEditor::~NewVerbTk1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

//==============================================================================
void NewVerbTk1AudioProcessorEditor::paint(juce::Graphics& g)
{
    // Fill background with gradient
    g.fillAll(juce::Colours::black);

    // Add a subtle gradient background
    juce::ColourGradient gradient(juce::Colour(10, 10, 30), 0.0f, 0.0f,
        juce::Colour(40, 40, 60), static_cast<float>(getWidth()), static_cast<float>(getHeight()),
        false);
    g.setGradientFill(gradient);
    g.fillRect(getLocalBounds());

    // Draw borders around sections
    g.setColour(juce::Colours::darkgrey);
    g.drawRect(getLocalBounds(), 1);

    // Draw panel label backgrounds
    g.setColour(juce::Colour(30, 30, 50).withAlpha(0.6f));

    // Main parameters section
    g.fillRoundedRectangle(20.0f, 50.0f, 660.0f, 140.0f, 10.0f);

    // Frequency band section
    g.fillRoundedRectangle(20.0f, 200.0f, 320.0f, 120.0f, 10.0f);

    // Draw section headers
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Main Parameters", 30, 60, 200, 20, juce::Justification::left, false);
    g.drawText("Frequency Bands", 30, 210, 200, 20, juce::Justification::left, false);
    g.drawText("Spectrogram", 30, 330, 200, 20, juce::Justification::left, false);
}

void NewVerbTk1AudioProcessorEditor::resized()
{
    // Position the title at the top
    titleLabel.setBounds(0, 10, getWidth(), 30);

    // Calculate positions for slider grid
    const int sliderSize = 80;
    const int mainParamSectionY = 80;
    const int bandSectionY = 230;

    // Position main parameter sliders
    wetDrySlider.setBounds(50, mainParamSectionY, sliderSize, sliderSize);
    wetDryLabel.setBounds(50, mainParamSectionY + sliderSize, sliderSize, 20);

    timeSlider.setBounds(150, mainParamSectionY, sliderSize, sliderSize);
    timeLabel.setBounds(150, mainParamSectionY + sliderSize, sliderSize, 20);

    densitySlider.setBounds(250, mainParamSectionY, sliderSize, sliderSize);
    densityLabel.setBounds(250, mainParamSectionY + sliderSize, sliderSize, 20);

    dampingSlider.setBounds(350, mainParamSectionY, sliderSize, sliderSize);
    dampingLabel.setBounds(350, mainParamSectionY + sliderSize, sliderSize, 20);

    sizeSlider.setBounds(450, mainParamSectionY, sliderSize, sliderSize);
    sizeLabel.setBounds(450, mainParamSectionY + sliderSize, sliderSize, 20);

    // Position freeze button
    freezeButton.setBounds(560, mainParamSectionY + 20, 80, 40);
    freezeLabel.setBounds(560, mainParamSectionY + sliderSize, 80, 20);

    // Position band sliders
    lowBandSlider.setBounds(50, bandSectionY, sliderSize, sliderSize);
    lowBandLabel.setBounds(50, bandSectionY + sliderSize, sliderSize, 20);

    midBandSlider.setBounds(150, bandSectionY, sliderSize, sliderSize);
    midBandLabel.setBounds(150, bandSectionY + sliderSize, sliderSize, 20);

    highBandSlider.setBounds(250, bandSectionY, sliderSize, sliderSize);
    highBandLabel.setBounds(250, bandSectionY + sliderSize, sliderSize, 20);

    // Position spectrogram
    spectrogramDisplay.setBounds(20, 350, 660, 130);
}

void NewVerbTk1AudioProcessorEditor::setupRotarySlider(juce::Slider& slider, juce::Label& label, const juce::String& labelText)
{
    // Configure slider
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    addAndMakeVisible(slider);

    // Configure label
    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.attachToComponent(&slider, false);
    addAndMakeVisible(label);
}

void NewVerbTk1AudioProcessorEditor::updateSpectrogramDisplay()
{
    spectrogramDisplay.update();
}

void NewVerbTk1AudioProcessorEditor::timerCallback()
{
    // Regular updates for UI components
    updateSpectrogramDisplay();
}