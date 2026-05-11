#include "PluginProcessor.h"
#include "PluginEditor.h"

TL64MagicEyeProcessor::TL64MagicEyeProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    driveParam = apvts.getRawParameterValue("drive");
    dynamicResponseParam = apvts.getRawParameterValue("dynamicResponse");
    responseFreqParam = apvts.getRawParameterValue("responseFreq");
    clippingParam = apvts.getRawParameterValue("clipping");
    bassCompParam = apvts.getRawParameterValue("bassComp");
    outputGainParam = apvts.getRawParameterValue("outputGain");
    oversampleParam = apvts.getRawParameterValue("oversampling");
    tubeDecayParam = apvts.getRawParameterValue("tubeDecay");
    gridVoltageParam = apvts.getRawParameterValue("gridVoltage");
    cathodeTempParam = apvts.getRawParameterValue("cathodeTemp");
    anodeCurrentParam = apvts.getRawParameterValue("anodeCurrent");
    panBiasParam = apvts.getRawParameterValue("panBias");
    spectrumTiltParam = apvts.getRawParameterValue("spectrumTilt");
}

juce::AudioProcessorValueTreeState::ParameterLayout TL64MagicEyeProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "drive", "Drive",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f, 0.3f), 6.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "dynamicResponse", "Dynamic Response", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "responseFreq", "Response Freq",
        juce::NormalisableRange<float>(20.0f, 1000.0f, 1.0f, 0.3f), 150.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "clipping", "Clipping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "bassComp", "Bass Compensation",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f, 0.3f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "outputGain", "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f, 0.3f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "oversampling", "Oversampling",
        juce::StringArray{ "1x", "2x", "4x", "8x" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "tubeDecay", "Tube Decay",
        juce::NormalisableRange<float>(0.2f, 3.0f, 0.01f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "gridVoltage", "Grid Voltage",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "cathodeTemp", "Cathode Temp",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "anodeCurrent", "Anode Current",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f, 0.3f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "panBias", "Pan Bias",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "spectrumTilt", "Spectrum Tilt",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

void TL64MagicEyeProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumInputChannels());

    const std::array<int, 4> factors = { 1, 2, 4, 8 };
    for (size_t i = 0; i < oversamplers.size(); ++i)
    {
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>>(static_cast<size_t>(factors[i]));
        oversamplers[i]->initProcessing(static_cast<size_t>(samplesPerBlock));
    }

    inputGain.prepare(spec);
    outputGain.prepare(spec);

    for (auto& filter : bassCompFilters)
        filter.prepare(spec);
    lastBassCompDb = 999.0f;

    for (auto& hp : highpassFilters)
    {
        hp.prepare(spec);
        // FIX: assign directly (no asterisk, no release)
        hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 800.0, 0.707);
    }

    float initialDecay = tubeDecayParam ? tubeDecayParam->load() : 1.0f;
    for (auto& cell : opticalCell)
        cell.prepare(static_cast<float>(sampleRate), initialDecay);

    const double smoothTime = 0.01;
    driveSmooth.reset(sampleRate, smoothTime);
    outputGainSmooth.reset(sampleRate, smoothTime);
    clipSmooth.reset(sampleRate, smoothTime);

    grMeterL.store(0.0f);
    grMeterR.store(0.0f);
    highFreqEnergy[0] = highFreqEnergy[1] = 0.0f;
}

void TL64MagicEyeProcessor::releaseResources()
{
    for (auto& os : oversamplers)
        if (os) os->reset();
}

void TL64MagicEyeProcessor::updateBassCompCoeffs(double sampleRate, float gainDb)
{
    if (std::abs(gainDb - lastBassCompDb) < 0.01f)
        return;

    for (auto& filter : bassCompFilters)
    {
        filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
            sampleRate, 120.0, 0.707, juce::Decibels::decibelsToGain(gainDb));
    }
    lastBassCompDb = gainDb;
}

inline float tubeWaveshape(float x, float drive, float cathodeTemp) noexcept
{
    float bias = cathodeTemp * 0.2f;
    float driven = (x + bias) * drive;
    float pos = std::tanh(driven);
    float neg = std::tanh(driven * 1.15f) * 0.92f;
    return (driven >= 0.0f) ? pos : neg;
}

inline float clipStage(float x, float amount) noexcept
{
    if (amount <= 0.001f) return x;
    float soft = x / (1.0f + std::abs(x));
    float hard = juce::jlimit(-1.0f, 1.0f, x);
    return x + (soft - x) * (1.0f - amount) + (hard - x) * amount;
}

void TL64MagicEyeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const double sampleRate = getSampleRate();

    float driveDb = driveParam->load();
    float outputDb = outputGainParam->load();
    float clipAmount = clippingParam->load();
    float bassDb = bassCompParam->load();
    float responseFreq = responseFreqParam->load();
    bool dynamicMode = dynamicResponseParam->load() > 0.5f;
    int osIndex = juce::jlimit(0, 3, static_cast<int>(oversampleParam->load()));

    float tubeDecay = tubeDecayParam->load();
    float gridVoltage = gridVoltageParam->load();
    float cathodeTemp = cathodeTempParam->load();
    float anodeDb = anodeCurrentParam->load();
    float panBias = panBiasParam->load();
    float spectrumTilt = spectrumTiltParam->load();

    // Update envelope time constants if decay changed
    static float lastDecay = tubeDecay;
    if (std::abs(tubeDecay - lastDecay) > 0.001f)
    {
        for (auto& cell : opticalCell)
        {
            cell.attackAlpha = std::exp(-1.0f / static_cast<float>(sampleRate * 0.003f * tubeDecay));
            cell.releaseAlpha = std::exp(-1.0f / static_cast<float>(sampleRate * 0.060f * tubeDecay));
        }
        lastDecay = tubeDecay;
    }

    driveSmooth.setTargetValue(juce::Decibels::decibelsToGain(driveDb));
    outputGainSmooth.setTargetValue(juce::Decibels::decibelsToGain(outputDb));
    clipSmooth.setTargetValue(clipAmount);
    updateBassCompCoeffs(sampleRate, bassDb);

    float lpCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi *
        responseFreq / static_cast<float>(sampleRate));

    float ratio = 2.0f + gridVoltage * 18.0f;
    const float threshold = 0.1f;
    float anodeGain = juce::Decibels::decibelsToGain(anodeDb);

    auto& currentOversampler = oversamplers[static_cast<size_t>(osIndex)];
    const int osFactor = currentOversampler->getOversamplingFactor();
    setLatencySamples(currentOversampler->getLatencyInSamples());

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        auto& cell = opticalCell[ch];
        auto& bassFilter = bassCompFilters[static_cast<size_t>(ch)];
        auto& highpass = highpassFilters[static_cast<size_t>(ch)];

        float peakGR = 0.0f;
        

        if (osFactor > 1)
        {
            juce::dsp::AudioBlock<float> inputBlock(buffer);
            auto osBlock = currentOversampler->processSamplesUp(inputBlock);
            float* osData = osBlock.getChannelPointer(static_cast<size_t>(ch));
            const size_t osSamples = osBlock.getNumSamples();

            for (size_t i = 0; i < osSamples; ++i)
            {
                float x = osData[i];
                float highPassed = highpass.processSample(x);
                float absHigh = std::abs(highPassed);
                highFreqEnergy[ch] = highFreqEnergy[ch] * 0.99f + absHigh * 0.01f;
                float weightedHigh = highFreqEnergy[ch];
                if (spectrumTilt > 0.0f)
                    weightedHigh *= (1.0f - spectrumTilt * 0.5f);
                else
                    weightedHigh *= (1.0f + spectrumTilt * 0.5f);
               

                float driven = x * driveSmooth.getNextValue();
                float bias = dynamicMode ? cell.envelope * 0.08f * (0.5f + cathodeTemp) : cathodeTemp * 0.2f;
                float tubeOut = tubeWaveshape(driven, 1.0f, bias);
                tubeOut *= anodeGain;

                float sidechain = cell.processSidechain(tubeOut, lpCoeff);
                float env = cell.processEnvelope(sidechain);
                float gr = cell.computeGainReduction(env, threshold, ratio);
                float leveled = tubeOut * gr;
                float grDb = -juce::Decibels::gainToDecibels(gr);
                peakGR = juce::jmax(peakGR, grDb);

                float clipped = clipStage(leveled, clipSmooth.getNextValue());
                osData[i] = clipped;
            }
            currentOversampler->processSamplesDown(inputBlock);

            for (int i = 0; i < numSamples; ++i)
            {
                float sample = data[i];
                sample = bassFilter.processSample(sample);
                data[i] = sample * outputGainSmooth.getNextValue();
            }
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float x = data[i];
                float highPassed = highpass.processSample(x);
                float absHigh = std::abs(highPassed);
                highFreqEnergy[ch] = highFreqEnergy[ch] * 0.99f + absHigh * 0.01f;
                float weightedHigh = highFreqEnergy[ch];
                if (spectrumTilt > 0.0f)
                    weightedHigh *= (1.0f - spectrumTilt * 0.5f);
                else
                    weightedHigh *= (1.0f + spectrumTilt * 0.5f);
              

                float driven = x * driveSmooth.getNextValue();
                float bias = dynamicMode ? cell.envelope * 0.08f * (0.5f + cathodeTemp) : cathodeTemp * 0.2f;
                float tubeOut = tubeWaveshape(driven, 1.0f, bias);
                tubeOut *= anodeGain;

                float sidechain = cell.processSidechain(tubeOut, lpCoeff);
                float env = cell.processEnvelope(sidechain);
                float gr = cell.computeGainReduction(env, threshold, ratio);
                float leveled = tubeOut * gr;
                float grDb = -juce::Decibels::gainToDecibels(gr);
                peakGR = juce::jmax(peakGR, grDb);

                float clipped = clipStage(leveled, clipSmooth.getNextValue());
                float bassOut = bassFilter.processSample(clipped);
                data[i] = bassOut * outputGainSmooth.getNextValue();
            }
        }

        float normGR = juce::jlimit(0.0f, 1.0f, peakGR / 20.0f);
        float meterAlpha = 0.1f;
        if (ch == 0)
            grMeterL.store(normGR * meterAlpha + grMeterL.load() * (1.0f - meterAlpha));
        else
            grMeterR.store(normGR * meterAlpha + grMeterR.load() * (1.0f - meterAlpha));
    }

    // Apply panBias to stereo image (simple gain)
    if (numChannels == 2 && std::abs(panBias) > 0.01f)
    {
        float leftGain = 1.0f - panBias * 0.5f;
        float rightGain = 1.0f + panBias * 0.5f;
        juce::AudioBuffer<float> temp(buffer);
        for (int i = 0; i < numSamples; ++i)
        {
            buffer.setSample(0, i, temp.getSample(0, i) * leftGain);
            buffer.setSample(1, i, temp.getSample(1, i) * rightGain);
        }
    }
}

void TL64MagicEyeProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml) copyXmlToBinary(*xml, destData);
}

void TL64MagicEyeProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* TL64MagicEyeProcessor::createEditor()
{
    return new TL64MagicEyeEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TL64MagicEyeProcessor();
}