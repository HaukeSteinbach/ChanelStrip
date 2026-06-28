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

    // ── EQ (3 Knobs, kubische Antwortkurve ±12 dB) ───────────────────────────
    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::HPF_ENABLED, "HPF 40 Hz", true));

    // Kubische NormalisableRange: subtil in der Mitte, exponentiell an den Rändern
    auto makeEQRange = []() -> NormalisableRange<float>
    {
        return NormalisableRange<float>(
            -12.0f, 12.0f,
            [](float, float, float v) -> float
            {
                const float t = v * 2.0f - 1.0f; // [0,1] → [-1,1]
                return t * t * t * 12.0f;        // kubisch → [-12,12] dB
            },
            [](float, float, float dB) -> float
            {
                const float t = std::cbrt(dB / 12.0f); // Umkehrung Kubik
                return (t + 1.0f) * 0.5f;              // → [0,1]
            });
    };

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::EQ_LOW, "Low  (100 Hz)", makeEQRange(), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::EQ_MID, "Mid  (1 kHz)", makeEQRange(), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::EQ_HIGH, "High (10 kHz)", makeEQRange(), 0.0f,
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
        ParamID::MORPH_AMOUNT, "Character",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<AudioParameterInt>(
        ParamID::WAVETABLE_IDX, "Wavetable", 0, 2, 0)); // 3 Sine-Presets

    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::LR_LINK, "L/R Link", true));

    // ── Console Mode (4 Gruppen: 0–3) ────────────────────────────────────────
    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::CONSOLE_ENABLE, "Console Mode", false));

    params.push_back(std::make_unique<AudioParameterInt>(
        ParamID::CONSOLE_GROUP, "Console Group", 0, 3, 0));

    // ── Output Clipper ────────────────────────────────────────────────────────
    // false = Hard clip, true = Neve-style soft clip (always active, max −4 dB)
    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::CLIPPER_MODE, "Soft Clip", false));

    return {params.begin(), params.end()};
}

// ── Konstruktor / Destruktor ──────────────────────────────────────────────────
SteinbachChanelStripAudioProcessor::SteinbachChanelStripAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Per-Instanz-Variation: analog console character
    // Jede Plugin-Instanz bekommt zufällige, unabhängige L/R-Charakteristika.
    // Link ON → L=R (Differenz aufgehoben).
    {
        std::mt19937 rng(std::random_device{}());

        // Gain-Variation ±0.6 dB pro Kanal → sichtbares Stereo-Image
        // σ = 0.3 dB: 68 % innerhalb ±0.3 dB, 95 % innerhalb ±0.6 dB
        std::normal_distribution<float> gainDist(0.0f, 0.55f);
        gainVarDbL = std::clamp(static_cast<float>(gainDist(rng)), -1.5f, 1.5f);
        gainVarDbR = std::clamp(static_cast<float>(gainDist(rng)), -1.5f, 1.5f);

        // Asymmetrie-Variation ±5 % → unterschiedliche 2nd-Harmonic-Kurven L vs R
        // DC-kompensiert im hot path, kein Offset auf dem Output
        std::uniform_real_distribution<float> asymDist(-0.15f, 0.15f);
        asymVarL = asymDist(rng);
        asymVarR = asymDist(rng);

        // variationL/R: kleine Werte für WavetableManager (kein Einfluss auf hot path)
        std::uniform_real_distribution<float> varDist(0.01f, 0.03f);
        variationL = varDist(rng);
        variationR = varDist(rng);
    }

    // Shared-Memory-Kanal reservieren (einmalig, non-RT)
    if (shmBlock.isValid())
        shmChannel = shmBlock.acquireChannel("CH");
}

SteinbachChanelStripAudioProcessor::~SteinbachChanelStripAudioProcessor()
{
    // Slots freigeben – außerhalb des Audio-Threads, daher sicher
    if (instanceSlot >= 0)
        InstanceRegistry::getInstance().releaseSlot(instanceSlot);
    if (shmChannel >= 0)
        shmBlock.releaseChannel(shmChannel);
}

void SteinbachChanelStripAudioProcessor::setChannelDisplayName(const juce::String &n)
{
    channelName = n;
    if (shmChannel >= 0)
        shmBlock.writeName(shmChannel, n.toRawUTF8());
}

void SteinbachChanelStripAudioProcessor::updateTrackProperties(const TrackProperties &props)
{
    if (props.name.has_value() && !props.name->isEmpty())
        setChannelDisplayName(*props.name);
}

// ── prepareToPlay ─────────────────────────────────────────────────────────────
void SteinbachChanelStripAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    eqProcessor.prepare(sampleRate, samplesPerBlock);

    // Oversampling initialisieren; Latenz dem Host melden (→ PDC)
    oversampling.initProcessing(static_cast<std::size_t>(samplesPerBlock));
    setLatencySamples(static_cast<int>(oversampling.getLatencyInSamples()));

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

    // Preamp / Pan Smoother (läuft auf OS-Rate = 2× native → getNextValue() im OS-Loop)
    const double osSr = sampleRate * 2.0;
    const float initPan = *apvts.getRawParameterValue(ParamID::PAN);
    const float initPreDb = *apvts.getRawParameterValue(ParamID::PREAMP_GAIN);
    smoothPreamp.reset(osSr, 0.020);
    smoothPanL.reset(osSr, 0.020);
    smoothPanR.reset(osSr, 0.020);
    smoothPreamp.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(initPreDb));
    smoothPanL.setCurrentAndTargetValue(std::cos((initPan + 1.0f) * 0.25f * juce::MathConstants<float>::pi));
    smoothPanR.setCurrentAndTargetValue(std::sin((initPan + 1.0f) * 0.25f * juce::MathConstants<float>::pi));
}

void SteinbachChanelStripAudioProcessor::releaseResources() {}

bool SteinbachChanelStripAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
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
    juce::AudioBuffer<float> &buffer, juce::MidiBuffer & /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Ungenutzte Output-Kanäle auf 0 setzen
    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    // ── Parameter lesen (einmal pro Block) ───────────────────────────────────
    bool hpfEnabled = *apvts.getRawParameterValue(ParamID::HPF_ENABLED) > 0.5f;
    float eqLow = *apvts.getRawParameterValue(ParamID::EQ_LOW);
    float eqMid = *apvts.getRawParameterValue(ParamID::EQ_MID);
    float eqHigh = *apvts.getRawParameterValue(ParamID::EQ_HIGH);
    float pan = *apvts.getRawParameterValue(ParamID::PAN);
    float preampGainDb = *apvts.getRawParameterValue(ParamID::PREAMP_GAIN);
    float morphAmount = *apvts.getRawParameterValue(ParamID::MORPH_AMOUNT);
    int wavetableIdx = static_cast<int>(*apvts.getRawParameterValue(ParamID::WAVETABLE_IDX));
    const bool lrLink = *apvts.getRawParameterValue(ParamID::LR_LINK) > 0.5f;
    const bool consoleOn = *apvts.getRawParameterValue(ParamID::CONSOLE_ENABLE) > 0.5f;
    const int consoleGroup = static_cast<int>(*apvts.getRawParameterValue(ParamID::CONSOLE_GROUP));
    const bool clipSoft = *apvts.getRawParameterValue(ParamID::CLIPPER_MODE) > 0.5f;

    // ── EQ ───────────────────────────────────────────────────────────────────
    // ── Shared-Memory: Parameter veroeffentlichen + Console-Override lesen ──────
    if (shmChannel >= 0)
    {
        if (auto *ch = shmBlock.get(shmChannel))
        {
            // Aktuellen Zustand fuer Console-Anzeige schreiben
            ch->preampDb.store(preampGainDb, std::memory_order_relaxed);
            ch->pan.store(pan, std::memory_order_relaxed);
            ch->eqLow.store(eqLow, std::memory_order_relaxed);
            ch->eqMid.store(eqMid, std::memory_order_relaxed);
            ch->eqHigh.store(eqHigh, std::memory_order_relaxed);
            ch->morph.store(morphAmount, std::memory_order_relaxed);
            ch->wavetable.store(wavetableIdx, std::memory_order_relaxed);
            ch->hpfEnabled.store(hpfEnabled ? 1u : 0u, std::memory_order_relaxed);

            // Console-Override anwenden wenn aktiv
            if (ch->hasOverride.load(std::memory_order_acquire))
            {
                preampGainDb = ch->ovPreampDb.load(std::memory_order_relaxed);
                pan = ch->ovPan.load(std::memory_order_relaxed);
                eqLow = ch->ovEqLow.load(std::memory_order_relaxed);
                eqMid = ch->ovEqMid.load(std::memory_order_relaxed);
                eqHigh = ch->ovEqHigh.load(std::memory_order_relaxed);
                morphAmount = ch->ovMorph.load(std::memory_order_relaxed);
                wavetableIdx = static_cast<int>(ch->ovWavetable.load(std::memory_order_relaxed));
                hpfEnabled = ch->ovHpfEnabled.load(std::memory_order_relaxed) != 0;
            }
        }
    }

    // ── EQ (mit evtl. ueberschriebenen Parametern) ───────────────────────────
    eqProcessor.setParameters(eqLow, eqMid, eqHigh, hpfEnabled);
    eqProcessor.process(buffer, numSamples);

    // ── Console Mode: Fremd-Pegel lesen ──────────────────────────────────────
    float crosstalk = 0.0f;
    float voltageSag = 1.0f;
    float morphMod = 0.0f;

    if (consoleOn && instanceSlot >= 0)
    {
        // Group-ID aktuell halten (user kann Combo zur Laufzeit ändern)
        InstanceRegistry::getInstance().setGroup(instanceSlot, consoleGroup);
        InstanceRegistry::getInstance().readConsoleState(
            instanceSlot, consoleGroup, crosstalk, voltageSag, morphMod);
    }

    // One-Pole Smoothing (~10 ms): verhindert Knackser durch Transientensprünge
    {
        const float tc = 0.010f; // 10 ms Zeitkonstante
        const float sr = static_cast<float>(getSampleRate());
        const float alpha = (sr > 0.0f)
                                ? 1.0f - std::exp(-static_cast<float>(numSamples) / (sr * tc))
                                : 1.0f;
        smoothedSag += alpha * (voltageSag - smoothedSag);
        smoothedMorphMod += alpha * (morphMod - smoothedMorphMod);
    }

    const float effectiveMorph = std::clamp(morphAmount + smoothedMorphMod, 0.0f, 1.0f);

    // ── Preamp / Wavefolding + Pan  (2x oversampled → kein Aliasing) ─────────
    // Smoother-Targets setzen: Interpolation läuft per OS-Sample im Loop unten
    smoothPreamp.setTargetValue(juce::Decibels::decibelsToGain(preampGainDb) * smoothedSag);
    smoothPanL.setTargetValue(std::cos((pan + 1.0f) * 0.25f * 3.14159265f));
    smoothPanR.setTargetValue(std::sin((pan + 1.0f) * 0.25f * 3.14159265f));

    // 1. Upsample auf 2x Samplerate
    juce::dsp::AudioBlock<float> nativeBlock(buffer);
    auto osBlock = oversampling.processSamplesUp(nativeBlock);
    const int numOSSamples = static_cast<int>(osBlock.getNumSamples());

    // ── Analytische tanh-Sättigung ────────────────────────────────────────────
    // Beide Kanäle nutzen denselben Drive (kein Level-Imbalance).
    // L/R-Charakteristik entsteht durch:
    //   1. gainVarDbL/R  → kleiner dB-Offset  → Stereo-Image im Imager sichtbar
    //   2. asymVarL/R    → DC-kompensierte Asymmetrie → verschiedene 2nd Harmonics
    // Link ON  → gainR = gainL, asymR = asymL  (Kanäle identisch)
    // Link OFF → unabhängige Werte, jede Instanz anders
    // ── Per-mode saturation ──────────────────────────────────────────────────────
    // 0 (Warm):   Neve transformer  – asymmetrisches tanh, Drive 2.0
    //             Betont 2nd + 3rd Harmonics, sehr warm und luftig
    // 1 (Medium): Tube Class-A      – hard-driven tanh + kubischer Blend, Drive 5.5
    //             Starkes 3rd Harmonic, harte Knie-Charakteristik ("grit")
    // 2 (Hot):    Sinusoidal Folder  – periodische sin-Projektion, Drive 2.8
    //             Reichhaltige Harmonics-Serie (3rd, 5th, 7th …), sehr "hot"
    const float gainL = juce::Decibels::decibelsToGain(gainVarDbL);
    const float gainR = lrLink ? gainL : juce::Decibels::decibelsToGain(gainVarDbR);

    const float asymL = lrLink ? 0.0f : asymVarL;
    const float asymR = lrLink ? 0.0f : asymVarR;

    // DC-Offset einmalig pro Block vorberechnen (verhindert DC nach Sättigung)
    auto computeDC = [&](float asym) noexcept -> float
    {
        switch (std::clamp(wavetableIdx, 0, 2))
        {
        case 0:
        {
            constexpr float kD = 2.0f;
            const float td = std::tanh(kD);
            return td > 1e-6f ? std::tanh(asym * kD) / td : 0.0f;
        }
        case 1:
        {
            constexpr float kD = 5.5f;
            const float td = std::tanh(kD);
            const float dt = td > 1e-6f ? std::tanh(asym * kD) / td : 0.0f;
            const float ac = std::clamp(asym, -1.0f, 1.0f);
            return 0.6f * dt + 0.4f * (ac * (1.5f - 0.5f * ac * ac));
        }
        case 2:
        {
            constexpr float kD = 2.8f;
            return std::sin(asym * kD * juce::MathConstants<float>::pi * 0.5f);
        }
        default:
            return 0.0f;
        }
    };
    const float dcL = computeDC(asymL);
    const float dcR = computeDC(asymR);

    // Per-Sample-Sättigungsfunktion (switch wird pro Block branch-predicted)
    auto saturate = [&](float x, float asym, float dc) noexcept -> float
    {
        switch (std::clamp(wavetableIdx, 0, 2))
        {
        case 0:
        {
            // Neve Transformer: weiches, warmes tanh, Drive=2.0
            // Betont 2nd + 3rd Harmonics; leichte Asymmetrie → Neve-Charakter
            constexpr float kD = 2.0f;
            const float td = std::tanh(kD);
            return td > 1e-6f ? std::tanh((x + asym) * kD) / td - dc : x;
        }
        case 1:
        {
            // Tube Class-A: harter Drive + kubischer Blend
            // Erzeugt kräftiges 3rd Harmonic und einen härteren Knie-Punkt ("grit")
            constexpr float kD = 5.5f;
            const float td = std::tanh(kD);
            const float t = td > 1e-6f ? std::tanh((x + asym) * kD) / td : x;
            const float xc = std::clamp(x + asym, -1.0f, 1.0f);
            return 0.6f * t + 0.4f * (xc * (1.5f - 0.5f * xc * xc)) - dc;
        }
        case 2:
        {
            // Sinusoidal Wavefolder: mehrfache sin-Projektion
            // Erzeugt dichte Harmonics-Reihe (3rd, 5th, 7th, ...) – sehr "hot"
            constexpr float kD = 2.8f;
            return std::sin((x + asym) * kD * juce::MathConstants<float>::pi * 0.5f) - dc;
        }
        default:
            return x;
        }
    };

    auto applyL = [&](float x) noexcept -> float
    {
        if (effectiveMorph < 1e-4f)
            return x * gainL;
        return (x + effectiveMorph * (saturate(x, asymL, dcL) - x)) * gainL;
    };
    auto applyR = [&](float x) noexcept -> float
    {
        if (effectiveMorph < 1e-4f)
            return x * gainR;
        return (x + effectiveMorph * (saturate(x, asymR, dcR) - x)) * gainR;
    };

    if (osBlock.getNumChannels() >= 2)
    {
        float *osL = osBlock.getChannelPointer(0);
        float *osR = osBlock.getChannelPointer(1);

        for (int n = 0; n < numOSSamples; ++n)
        {
            const float curPreamp = smoothPreamp.getNextValue();
            const float curPanL = smoothPanL.getNextValue();
            const float curPanR = smoothPanR.getNextValue();

            const float inL = osL[n] * curPreamp;
            const float inR = osR[n] * curPreamp;

            const float foldedL = applyL(inL);
            const float foldedR = applyR(inR);

            osL[n] = foldedL * curPanL + foldedR * crosstalk;
            osR[n] = foldedR * curPanR + foldedL * crosstalk;
        }
    }

    // 2. Downsample zurück auf native Samplerate
    oversampling.processSamplesDown(nativeBlock);
    // ── Output Clipper – immer aktiv, nach Downsampling ───────────────────────
    // Hard: harter Schnitt bei ±4 dB (≈1.585 linear) – transparent, kein Überschwinger
    // Soft: Neve-Transformer-Charakter – lineares Verhalten bis 70% des Schwellwerts,
    //       danach weiche tanh-Sättigung; nähert sich asymptotisch ±4 dB an
    {
        constexpr float kClipThresh = 0.63096f; // -4 dB = 10^(-4/20)
        constexpr float kKnee = kClipThresh * 0.70f;
        constexpr float kRange = kClipThresh - kKnee;

        auto clip = [&](float x) noexcept -> float
        {
            if (clipSoft)
            {
                const float xa = std::fabs(x);
                if (xa <= kKnee)
                    return x;
                const float sgn = x >= 0.0f ? 1.0f : -1.0f;
                return sgn * (kKnee + kRange * std::tanh((xa - kKnee) / kRange));
            }
            // Hard clip
            return x < -kClipThresh  ? -kClipThresh
                   : x > kClipThresh ? kClipThresh
                                     : x;
        };

        float *chL = buffer.getWritePointer(0);
        float *chR = buffer.getWritePointer(1);
        for (int n = 0; n < numSamples; ++n)
        {
            chL[n] = clip(chL[n]);
            chR[n] = clip(chR[n]);
        }
    }
    // ── Console Mode: eigenen RMS schreiben (nach Downsample) ────────────────
    if (consoleOn && instanceSlot >= 0)
    {
        const float rmsL = computeRMS(buffer.getReadPointer(0), numSamples);
        const float rmsR = computeRMS(buffer.getReadPointer(1), numSamples);
        InstanceRegistry::getInstance().writeRMS(instanceSlot, rmsL, rmsR);
    }
}

// ── State: Speichern / Laden ──────────────────────────────────────────────────
void SteinbachChanelStripAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SteinbachChanelStripAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────
float SteinbachChanelStripAudioProcessor::computeRMS(const float *data, int numSamples) noexcept
{
    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        sum += data[i] * data[i];

    return std::sqrt(sum / static_cast<float>(numSamples));
}

void SteinbachChanelStripAudioProcessor::loadCustomWavetable(
    const float *data, int numRows, int numCols)
{
    // Non-RT: darf von Message Thread aufgerufen werden
    wavetableManagerL.loadCustomWavetable(data, numRows, numCols);
    wavetableManagerR.loadCustomWavetable(data, numRows, numCols);
    wavetableManagerL.applyInstanceVariation(variationL);
    wavetableManagerR.applyInstanceVariation(variationR);
}

// ── Factory-Funktion ─────────────────────────────────────────────────────────
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new SteinbachChanelStripAudioProcessor();
}
