#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <atomic>

class TL64MagicEyeProcessor : public juce::AudioProcessor
{
public:
    TL64MagicEyeProcessor();
    ~TL64MagicEyeProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TL-64 Magic Eye"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    float getLeftGRMeter() const { return grMeterL.load(); }
    float getRightGRMeter() const { return grMeterR.load(); }
    float getHighFreqEnergyLeft() const { return highFreqEnergy[0]; }
    float getHighFreqEnergyRight() const { return highFreqEnergy[1]; }
    float getHighFreqPeakLeft() const { return highFreqPeak[0]; }
    float getHighFreqPeakRight() const { return highFreqPeak[1]; }

    // Get current tube parameter values (for GUI drag start)
    float getCurrentCathodeTemp() const { return cathodeTempParam ? cathodeTempParam->load() : 0.5f; }
    float getCurrentGridVoltage() const { return gridVoltageParam ? gridVoltageParam->load() : 0.5f; }
    float getCurrentAnodeCurrent() const { return anodeCurrentParam ? anodeCurrentParam->load() : 0.0f; }

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 4> oversamplers;

    struct OpticalCell
    {
        float envelope = 0.0f;
        float attackAlpha = 0.0f;
        float releaseAlpha = 0.0f;
        float lpState = 0.0f;

        void prepare(float sampleRate, float decayMult = 1.0f)
        {
            attackAlpha = std::exp(-1.0f / (sampleRate * 0.003f * decayMult));
            releaseAlpha = std::exp(-1.0f / (sampleRate * 0.060f * decayMult));
        }

        float processSidechain(float input, float lpCoeff) noexcept
        {
            lpState += lpCoeff * (input - lpState);
            return input - lpState;
        }

        float processEnvelope(float input) noexcept
        {
            float target = std::abs(input);
            float alpha = (target > envelope) ? attackAlpha : releaseAlpha;
            envelope = target + alpha * (envelope - target);
            return envelope;
        }

        float computeGainReduction(float env, float threshold, float ratio) const noexcept
        {
            if (env <= threshold) return 1.0f;
            float envDb = juce::Decibels::gainToDecibels(env);
            float thrDb = juce::Decibels::gainToDecibels(threshold);
            float grDb = (thrDb - envDb) * (1.0f - 1.0f / ratio);
            return juce::Decibels::decibelsToGain(grDb);
        }
    };

    OpticalCell opticalCell[2];

    // Gains
    juce::dsp::Gain<float> inputGain;   // not heavily used but kept for completeness
    juce::dsp::Gain<float> outputGain;

    std::array<juce::dsp::IIR::Filter<float>, 2> bassCompFilters;
    float lastBassCompDb = 999.0f;
    void updateBassCompCoeffs(double sampleRate, float gainDb);

    std::array<juce::dsp::IIR::Filter<float>, 2> highpassFilters;
    float highFreqEnergy[2] = { 0.0f, 0.0f };
    float highFreqPeak[2] = { 0.0f, 0.0f };
    float spectrumWeight[2] = { 0.0f, 0.0f };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> driveSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGainSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         clipSmooth;

    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* dynamicResponseParam = nullptr;
    std::atomic<float>* responseFreqParam = nullptr;
    std::atomic<float>* clippingParam = nullptr;
    std::atomic<float>* bassCompParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* oversampleParam = nullptr;

    std::atomic<float>* tubeDecayParam = nullptr;
    std::atomic<float>* gridVoltageParam = nullptr;
    std::atomic<float>* cathodeTempParam = nullptr;
    std::atomic<float>* anodeCurrentParam = nullptr;
    std::atomic<float>* panBiasParam = nullptr;
    std::atomic<float>* spectrumTiltParam = nullptr;

    std::atomic<float> grMeterL{ 0.0f }, grMeterR{ 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TL64MagicEyeProcessor)
};