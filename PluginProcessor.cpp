#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NewVerbTk1AudioProcessor::NewVerbTk1AudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
    ),
    forwardFFT(fftOrder),
    inverseFFT(fftOrder),
    parameters(*this, nullptr, "PARAMETERS", createParameters())
{
    // Get references to parameters
    wetDryParameter = parameters.getRawParameterValue("wet_dry");
    timeParameter = parameters.getRawParameterValue("time");
    densityParameter = parameters.getRawParameterValue("density");
    dampingParameter = parameters.getRawParameterValue("damping");
    sizeParameter = parameters.getRawParameterValue("size");
    lowBandParameter = parameters.getRawParameterValue("low_band");
    midBandParameter = parameters.getRawParameterValue("mid_band");
    highBandParameter = parameters.getRawParameterValue("high_band");
    freezeParameter = parameters.getRawParameterValue("freeze");

    // Initialize buffers
    fftWorkingBuffer.resize(fftSize * 2, 0.0f); // Real + Imaginary
    windowBuffer.resize(fftSize, 0.0f);
    spectralMagnitudeBuffer.resize(fftSize / 2, 0.0f);

    fftTimeDomainBuffer.resize(fftSize, std::complex<float>(0.0f, 0.0f));
    fftFrequencyDomainBuffer.resize(fftSize, std::complex<float>(0.0f, 0.0f));
    fftConvolutionBuffer.resize(fftSize, std::complex<float>(0.0f, 0.0f));

    // Initialize Hann window
    for (int i = 0; i < fftSize; ++i)
        windowBuffer[i] = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1));

    // Start timer for spectrogram updates
    startTimerHz(30); // Update 30 times per second
}

NewVerbTk1AudioProcessor::~NewVerbTk1AudioProcessor()
{
    stopTimer();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout NewVerbTk1AudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Add parameters to control the spectral processing
    params.push_back(std::make_unique<juce::AudioParameterFloat>("wet_dry", "Wet/Dry", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("time", "Time", 0.1f, 10.0f, 2.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("density", "Density", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("damping", "Damping", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("size", "Size", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("low_band", "Low Band", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mid_band", "Mid Band", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("high_band", "High Band", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("freeze", "Freeze", false));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String NewVerbTk1AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NewVerbTk1AudioProcessor::acceptsMidi() const
{
    return false;
}

bool NewVerbTk1AudioProcessor::producesMidi() const
{
    return false;
}

bool NewVerbTk1AudioProcessor::isMidiEffect() const
{
    return false;
}

double NewVerbTk1AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NewVerbTk1AudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int NewVerbTk1AudioProcessor::getCurrentProgram()
{
    return 0;
}

void NewVerbTk1AudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String NewVerbTk1AudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void NewVerbTk1AudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void NewVerbTk1AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Initialize processing buffers
    fftInputBuffer.setSize(2, fftSize * 2); // stereo buffer with enough room for overlapping
    fftInputBuffer.clear();

    fftOutputBuffer.setSize(2, fftSize * 2);
    fftOutputBuffer.clear();

    fifoIndex = 0;
    nextFFTBlockReady = false;
}

void NewVerbTk1AudioProcessor::releaseResources()
{
    // Free resources when not playing
    fftInputBuffer.setSize(0, 0);
    fftOutputBuffer.setSize(0, 0);
}

bool NewVerbTk1AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // We support mono or stereo for both input and output
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Input and output must be the same
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void NewVerbTk1AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that we're not using
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Get current parameter values
    wetDry = wetDryParameter->load();
    time = timeParameter->load();
    density = densityParameter->load();
    damping = dampingParameter->load();
    size = sizeParameter->load();
    lowBand = lowBandParameter->load();
    midBand = midBandParameter->load();
    highBand = highBandParameter->load();
    freeze = freezeParameter->load() > 0.5f;

    // Keep the original signal for wet/dry mixing
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // Process each stereo channel
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const float* inputData = buffer.getReadPointer(channel);
        float* outputData = buffer.getWritePointer(channel);

        // Add new samples to FFT input buffer
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // Add the sample to the FFT input buffer with overlap
            fftInputBuffer.setSample(channel, fifoIndex, inputData[sample]);

            // Process when we have enough samples
            if (fifoIndex >= hopSize)
            {
                // We have enough samples to process the next FFT block
                nextFFTBlockReady = true;
            }

            // Move to the next sample in our FIFO
            fifoIndex = (fifoIndex + 1) % fftSize;

            // If we have processed data available, output it
            if (fifoIndex % hopSize == 0)
            {
                // Perform the spectral processing if needed
                if (nextFFTBlockReady)
                {
                    // Copy data from the input buffer for processing
                    for (int i = 0; i < fftSize; ++i)
                    {
                        int circularIndex = (fifoIndex + i) % fftSize;
                        float windowedSample = fftInputBuffer.getSample(channel, circularIndex) * windowBuffer[i];
                        fftTimeDomainBuffer[i] = std::complex<float>(windowedSample, 0.0f);
                    }

                    // Forward FFT - convert to using JUCE's FFT interface properly
                    std::vector<float> fftInOut(fftSize * 2, 0.0f);

                    // Copy real data to the real part of the FFT input
                    for (int i = 0; i < fftSize; ++i)
                    {
                        fftInOut[i * 2] = fftTimeDomainBuffer[i].real();
                        fftInOut[i * 2 + 1] = 0.0f; // Imaginary part is zero
                    }

                    // Perform forward FFT (in-place)
                    forwardFFT.performRealOnlyForwardTransform(fftInOut.data(), false);

                    // Convert back to our complex format for processing
                    for (int i = 0; i < fftSize; ++i)
                    {
                        fftFrequencyDomainBuffer[i] = std::complex<float>(fftInOut[i * 2], fftInOut[i * 2 + 1]);
                    }

                    // Apply spectral processing
                    applySpectralProcessing(fftFrequencyDomainBuffer);

                    // Inverse FFT - convert to using JUCE's FFT interface properly
                    std::vector<float> ifftInOut(fftSize * 2, 0.0f);

                    // Copy complex data to the FFT input
                    for (int i = 0; i < fftSize; ++i)
                    {
                        ifftInOut[i * 2] = fftFrequencyDomainBuffer[i].real();
                        ifftInOut[i * 2 + 1] = fftFrequencyDomainBuffer[i].imag();
                    }

                    // Perform inverse FFT (in-place)
                    inverseFFT.performRealOnlyInverseTransform(ifftInOut.data());

                    // Convert back to our complex format
                    for (int i = 0; i < fftSize; ++i)
                    {
                        fftTimeDomainBuffer[i] = std::complex<float>(ifftInOut[i * 2], 0.0f);
                    }

                    // Store processed data in output buffer for overlap-add
                    for (int i = 0; i < fftSize; ++i)
                    {
                        int outputIndex = (fifoIndex + i) % fftSize;
                        float processedSample = fftTimeDomainBuffer[i].real() * windowBuffer[i] / (fftSize / 2.0f);

                        // Overlap-add
                        float currentSample = fftOutputBuffer.getSample(channel, outputIndex);
                        fftOutputBuffer.setSample(channel, outputIndex, currentSample + processedSample);
                    }

                    nextFFTBlockReady = false;
                }
            }

            // Copy from the output buffer to the actual output
            outputData[sample] = fftOutputBuffer.getSample(channel, fifoIndex);

            // Clear the sample we just read
            fftOutputBuffer.setSample(channel, fifoIndex, 0.0f);
        }
    }

    // Apply wet/dry mix
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        const float* dryData = dryBuffer.getReadPointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] = (1.0f - wetDry) * dryData[sample] + wetDry * channelData[sample];
        }
    }

    // Update the spectrogram data for the GUI
    updateSpectrogramBuffers();
}

void NewVerbTk1AudioProcessor::applySpectralProcessing(std::vector<std::complex<float>>& fftData)
{
    // Spectral processing based on our parameters
    const int numBins = fftSize / 2;
    const float lowCutoff = numBins * 0.1f; // 10% of spectrum
    const float midCutoff = numBins * 0.4f; // 40% of spectrum

    for (int i = 1; i < numBins; ++i)  // Skip DC
    {
        // Determine which band this bin belongs to
        float bandMultiplier = 1.0f;

        if (i < lowCutoff)              // Low band
            bandMultiplier = lowBand;
        else if (i < midCutoff)         // Mid band
            bandMultiplier = midBand;
        else                            // High band
            bandMultiplier = highBand;

        if (!freeze)
        {
            // Calculate magnitude and phase
            float mag = std::abs(fftData[i]);
            float phase = std::arg(fftData[i]);

            // Apply spectral transformations based on parameters
            // Size parameter affects bin spreading/smearing
            if (size > 0.01f)
            {
                int spreadAmount = static_cast<int>(size * 10.0f);
                if (spreadAmount > 0 && i + spreadAmount < numBins)
                {
                    for (int j = 1; j <= spreadAmount; ++j)
                    {
                        float spreadFactor = (spreadAmount - j + 1) / static_cast<float>(spreadAmount + 1);
                        int targetBin = i + j;
                        fftData[targetBin] += fftData[i] * spreadFactor * 0.3f;
                    }
                }
            }

            // Time parameter affects decay time of frequency bins
            float decayFactor = 1.0f - (1.0f / (time * 10.0f + 1.0f));

            // Damping affects higher frequencies more
            float dampingFactor = 1.0f - (damping * float(i) / float(numBins));
            dampingFactor = juce::jmax(0.01f, dampingFactor);

            // Density adds random fluctuations
            float densityFactor = 1.0f;
            if (density > 0.01f)
            {
                float random = 0.5f + 0.5f * std::sin(i * 0.3f + juce::Time::getMillisecondCounter() * 0.001f);
                densityFactor = 1.0f - (density * 0.3f * random);
            }

            // Apply all effects
            float finalMultiplier = bandMultiplier * decayFactor * dampingFactor * densityFactor;

            // Convert back to complex
            fftData[i] *= finalMultiplier;

            // Mirror to maintain symmetry for real signals
            fftData[fftSize - i] = std::conj(fftData[i]);
        }
    }
}

void NewVerbTk1AudioProcessor::updateSpectrogramBuffers()
{
    juce::SpinLock::ScopedLockType scopedLock(spectralDataLock);

    // Calculate magnitude spectrum for visualization
    for (int i = 0; i < fftSize / 2; ++i)
    {
        spectralMagnitudeBuffer[i] = std::abs(fftFrequencyDomainBuffer[i]);
    }
}

void NewVerbTk1AudioProcessor::timerCallback()
{
    // This is just to ensure we regularly update the UI with new spectral data
    if (auto* editor = dynamic_cast<NewVerbTk1AudioProcessorEditor*>(getActiveEditor()))
    {
        editor->updateSpectrogramDisplay();
    }
}

//==============================================================================
bool NewVerbTk1AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NewVerbTk1AudioProcessor::createEditor()
{
    return new NewVerbTk1AudioProcessorEditor(*this, parameters);
}

//==============================================================================
void NewVerbTk1AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Store current plugin state
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void NewVerbTk1AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Restore plugin state
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewVerbTk1AudioProcessor();
}