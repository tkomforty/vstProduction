#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * NewVerbTk1 Audio Processor
 * A spectral processing reverb plugin with customizable frequency manipulation.
 */
class NewVerbTk1AudioProcessor : public juce::AudioProcessor,
    private juce::Timer
{
public:
    //==============================================================================
    NewVerbTk1AudioProcessor();
    ~NewVerbTk1AudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Custom methods for our spectral processor

    enum SpectralParams
    {
        WET_DRY = 0,
        TIME,
        DENSITY,
        DAMPING,
        SIZE,
        LOW_BAND,
        MID_BAND,
        HIGH_BAND,
        FREEZE,
        TOTAL_NUM_PARAMS
    };

    // FFT Parameters
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;

    // For editor to access spectral data
    const float* getSpectralMagnitudeBuffer() const { return spectralMagnitudeBuffer.data(); }
    int getFFTSize() const { return fftSize; }

    // Audio parameter tree
    juce::AudioProcessorValueTreeState parameters;

private:
    //==============================================================================
    // Private FFT processing methods
    void timerCallback() override;
    void updateSpectrogramBuffers();

    // Parameter connections
    float wetDry, time, density, damping, size, lowBand, midBand, highBand, freeze;
    std::atomic<float>* wetDryParameter = nullptr;
    std::atomic<float>* timeParameter = nullptr;
    std::atomic<float>* densityParameter = nullptr;
    std::atomic<float>* dampingParameter = nullptr;
    std::atomic<float>* sizeParameter = nullptr;
    std::atomic<float>* lowBandParameter = nullptr;
    std::atomic<float>* midBandParameter = nullptr;
    std::atomic<float>* highBandParameter = nullptr;
    std::atomic<float>* freezeParameter = nullptr;

    // FFT objects
    juce::dsp::FFT forwardFFT;
    juce::dsp::FFT inverseFFT;

    // Processing buffers
    juce::AudioBuffer<float> fftInputBuffer;
    juce::AudioBuffer<float> fftOutputBuffer;

    std::vector<float> fftWorkingBuffer;
    std::vector<float> windowBuffer;
    std::vector<float> spectralMagnitudeBuffer;

    std::vector<std::complex<float>> fftTimeDomainBuffer;
    std::vector<std::complex<float>> fftFrequencyDomainBuffer;
    std::vector<std::complex<float>> fftConvolutionBuffer;

    // Internal processing state
    juce::SpinLock spectralDataLock;
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;

    // Helper methods
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    void applySpectralProcessing(std::vector<std::complex<float>>& fftData);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewVerbTk1AudioProcessor)
};