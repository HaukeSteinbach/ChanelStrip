#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <algorithm>
#include <random>

// ── Parameter Layout ──────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
SteinbachChanelStripAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // ── EQ ───────────────────────────────────────────────────────────────────
    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::HPF_ENABLED, "HPF 40 Hz", false));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::LS_FREQ, "Low Shelf Freq",
        NormalisableRange<float>(20.0f, 600.0f, 1.0f, 0.4f), 80.0f,
        AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::LS_GAIN, "Low Shelf Gain",
        NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::MID_FREQ, "Mid Freq",
        NormalisableRange<float>(200.0f, 8000.0f, 1.0f, 0.4f), 1000.0f,
        AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::MID_GAIN, "Mid Gain",
        NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::MID_Q, "Mid Q",
        NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 0.707f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::HS_FREQ, "High Shelf Freq",
        NormalisableRange<float>(2000.0f, 20000.0f, 1.0f, 0.4f), 8000.0f,
        AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::HS_GAIN, "High Shelf Gain",
        NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    // ── Routing ──────────────────────────────────────────────────────────────
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::PAN, "Pan",
        NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

    // ── Preamp ───────────────────────────────────────────────────────────────
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::PREAMP_GAIN, "Preamp Gain",
        NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::MORPH_AMOUNT, "Wavefolding",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<AudioParameterInt>(
        ParamID::WAVETABLE_IDX, "Wavetable", 0, WavetableManager::kMaxPresets - 1, 0));

    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::LR_LINK, "L/R Link Variation", true));

    // ── Console Mode ─────────────────────────────────────────────────────────
    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::CONSOLE_ENABLE, "Analog Console Mode", false));

    params.push_back(std::make_unique<AudioParameterInt>(
        ParamID::CONSOLE_GROUP, "Console Group", 0, 7, 0));

    return { params.begin(), params.end() };
}

// ── Konstruktor / Destruktor ──────────────────────────────────────────────────
SteinbachChanelStripAudioProcessor::SteinbachChanelStripAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Per-Instanz-Variation: L und R bekommen verschiedene zufällige Abweichungen
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.01f, 0.05f); // 1..5 %
    variationL = dist(rng);
    variationR = dist(rng);
}

SteinbachChanelStripAudioProcessor::~SteinbachChanelStripAudioProcessor()
{
    // Slot freigeben – außerhalb des Audio-Threads, daher sicher
    if (instanceSlot >= 0)
        InstanceRegistry::getInstance().releaseSlot(instanceSlot);
}

// ── prepareToPlay ─────────────────────────────────────────────────────────────
void SteinbachChanelStripAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    eqProcessor.prepare(sampleRate, samplesPerBlock);

    // Wavetable-Variation einmalig anwenden
    wavetableManagerL.loadPreset(0);
    wavetableManagerL.applyInstanceVariation(variationL);

    wavetableManagerR.loadPreset(0);
    wavetableManagerR.applyInstanceVariation(variationR);

    // Console-Mode Slot reservieren (wenn noch nicht geschehen)
    if (instanceSlot < 0)
    {
        const int group = static_cast<int>(
            *apvts.getRawParameterValue(ParamID::CONSOLE_GROUP));
        instanceSlot = InstanceRegistry::getInstance().acquireSlot(group);
    }
}

void SteinbachChanelStripAudioProcessor::releaseResources() {}

bool SteinbachChanelStripAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

// ── processBlock ──────────────────────────────────────────────────────────────
//
// AUDIO THREAD: KEINE Allokationen, KEINE Locks, KEINE File I/O.
// Alle atomaren Zugriffe auf InstanceRegistry sind explizit memory_order_relaxed
// (Lesen/Schreiben der eigenen Werte) oder memory_order_acquire (Lesen fremder).
//
void SteinbachChanelStripAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples             = buffer.getNumSamples();

    // Ungenutzte Output-Kanäle auf 0 setzen
    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    // ── Parameter lesen (einmal pro Block) ───────────────────────────────────
    const bool  hpfEnabled   = *apvts.getRawParameterValue(ParamID::HPF_ENABLED) > 0.5f;
    const float lsFreq       = *apvts.getRawParameterValue(ParamID::LS_FREQ);
    const float lsGain       = *apvts.getRawParameterValue(ParamID::LS_GAIN);
    const float midFreq      = *apvts.getRawParameterValue(ParamID::MID_FREQ);
    const float midGain      = *apvts.getRawParameterValue(ParamID::MID_GAIN);
    const float midQ         = *apvts.getRawParameterValue(ParamID::MID_Q);
    const float hsFreq       = *apvts.getRawParameterValue(ParamID::HS_FREQ);
    const float hsGain       = *apvts.getRawParameterValue(ParamID::HS_GAIN);
    const float pan          = *apvts.getRawParameterValue(ParamID::PAN);
    const float preampGainDb = *apvts.getRawParameterValue(ParamID::PREAMP_GAIN);
    const float morphAmount  = *apvts.getRawParameterValue(ParamID::MORPH_AMOUNT);
    const bool  lrLink       = *apvts.getRawParameterValue(ParamID::LR_LINK) > 0.5f;
    const bool  consoleOn    = *apvts.getRawParameterValue(ParamID::CONSOLE_ENABLE) > 0.5f;
    const int   consoleGroup = static_cast<int>(*apvts.getRawParameterValue(ParamID::CONSOLE_GROUP));

    // ── EQ ───────────────────────────────────────────────────────────────────
    eqProcessor.setParameters(hpfEnabled,
                               lsFreq, lsGain,
                               midFreq, midGain, midQ,
                               hsFreq, hsGain);
    eqProcessor.process(buffer, numSamples);

    // ── Console Mode: Fremd-Pegel lesen ──────────────────────────────────────
    float crosstalk = 0.0f;
    float voltageSag = 1.0f;

    if (consoleOn && instanceSlot >= 0)
    {
        InstanceRegistry::getInstance().readConsoleState(
            instanceSlot, consoleGroup, crosstalk, voltageSag);
    }

    // ── Preamp / Wavefolding + Pan ────────────────────────────────────────────
    const float preampLinear = juce::Decibels::decibelsToGain(preampGainDb) * voltageSag;

    // Stereo-Pan (linear, Equal-Power-Annäherung)
    const float panL = std::cos((pan + 1.0f) * 0.25f * 3.14159265f);
    const float panR = std::sin((pan + 1.0f) * 0.25f * 3.14159265f);

    if (buffer.getNumChannels() >= 2)
    {
        float* dataL = buffer.getWritePointer(0);
        float* dataR = buffer.getWritePointer(1);

        const WavetableManager& wtL = wavetableManagerL;
        // R nutzt L-Variation wenn LR-Link aktiv
        const WavetableManager& wtR = lrLink ? wavetableManagerL : wavetableManagerR;

        for (int n = 0; n < numSamples; ++n)
        {
            // Preamp-Gain anwenden
            const float inL = dataL[n] * preampLinear;
            const float inR = dataR[n] * preampLinear;

            // Wavefolding via Wavetable
            const float foldedL = wtL.processSample(inL, morphAmount);
            const float foldedR = wtR.processSample(inR, morphAmount);

            // Crosstalk hinzumischen (Console Mode)
            const float outL = foldedL * panL + foldedR * crosstalk;
            const float outR = foldedR * panR + foldedL * crosstalk;

            dataL[n] = outL;
            dataR[n] = outR;
        }

        // ── Console Mode: eigenen RMS schreiben ──────────────────────────────
        if (consoleOn && instanceSlot >= 0)
        {
            const float rmsL = computeRMS(dataL, numSamples);
            const float rmsR = computeRMS(dataR, numSamples);
            InstanceRegistry::getInstance().writeRMS(instanceSlot, rmsL, rmsR);
        }
    }
}

// ── State: Speichern / Laden ──────────────────────────────────────────────────
void SteinbachChanelStripAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SteinbachChanelStripAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────
float SteinbachChanelStripAudioProcessor::computeRMS(const float* data, int numSamples) noexcept
{
    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        sum += data[i] * data[i];

    return std::sqrt(sum / static_cast<float>(numSamples));
}

void SteinbachChanelStripAudioProcessor::loadCustomWavetable(
    const float* data, int numRows, int numCols)
{
    // Non-RT: darf von Message Thread aufgerufen werden
    wavetableManagerL.loadCustomWavetable(data, numRows, numCols);
    wavetableManagerR.loadCustomWavetable(data, numRows, numCols);
    wavetableManagerL.applyInstanceVariation(variationL);
    wavetableManagerR.applyInstanceVariation(variationR);
}

// ── Factory-Funktion ─────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SteinbachChanelStripAudioProcessor();
}
