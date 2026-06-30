#pragma once

#include <JuceHeader.h>
#include "DSP/EQProcessor.h"
#include "DSP/WavetableManager.h"
#include "ConsoleMode/InstanceRegistry.h"
#include "SharedMemory/SharedParameterBlock.h"

// ── Parameter-IDs ─────────────────────────────────────────────────────────────
namespace ParamID
{
    // EQ – 3 Knobs, feste Frequenzen (100 Hz / 1 kHz / 10 kHz)
    static constexpr auto HPF_ENABLED = "hpf_enabled";
    static constexpr auto EQ_LOW = "eq_low";   // Low Shelf ±12 dB
    static constexpr auto EQ_MID = "eq_mid";   // Mid Bell  ±12 dB
    static constexpr auto EQ_HIGH = "eq_high"; // High Shelf ±12 dB

    // Routing
    static constexpr auto PAN = "pan";
    static constexpr auto BINAURAL_PAN = "binaural_pan";

    // Preamp / Wavefolding
    static constexpr auto PREAMP_GAIN = "preamp_gain";
    static constexpr auto MORPH_AMOUNT = "morph_amount";
    static constexpr auto WAVETABLE_IDX = "wavetable_idx";
    static constexpr auto LR_LINK = "lr_link";

    // Console Mode (max. 4 Gruppen: 0–3)
    static constexpr auto CONSOLE_ENABLE = "console_enable";
    static constexpr auto CONSOLE_GROUP = "console_group";

    // Output Clipper
    static constexpr auto CLIPPER_MODE = "clipper_mode"; // false=hard, true=soft

    // Sub Left: Linken Sub (< 77 Hz) auf rechten Kanal spiegeln
    static constexpr auto SUB_LEFT = "sub_left";
}

// ── Processor ─────────────────────────────────────────────────────────────────
class SteinbachChanelStripAudioProcessor : public juce::AudioProcessor
{
public:
    SteinbachChanelStripAudioProcessor();
    ~SteinbachChanelStripAudioProcessor() override;

    // ── AudioProcessor API ───────────────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    // ── Public State ─────────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** Lade eigene Wavetable (non-RT, z.B. aus FileChooser). */
    void loadCustomWavetable(const float *data, int numRows, int numCols);

    // ── Console / SharedMemory API ────────────────────────────────────────────
    SharedParameterBlock &getSharedBlock() noexcept { return shmBlock; }
    int getShmChannel() const noexcept { return shmChannel; }
    juce::String getChannelDisplayName() const { return channelName; }
    void setChannelDisplayName(const juce::String &n);
    void updateTrackProperties(const TrackProperties &props) override;

private:
    // ── DSP ──────────────────────────────────────────────────────────────────
    EQProcessor eqProcessor;
    WavetableManager wavetableManagerL;
    WavetableManager wavetableManagerR;

    // 2x Oversampling – nur für den Waveshaper-Abschnitt.
    // Latenz: ~11 Samples @ 44.1 kHz (wird dem Host gemeldet → PDC).
    juce::dsp::Oversampling<float> oversampling{
        2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR};

    // ── Console Mode ─────────────────────────────────────────────────────────
    int instanceSlot{-1};

    // ── Instanz-Variation (analog console character) ─────────────────────────
    float variationL{0.0f}; // für WavetableManager (kein hot-path Einfluss)
    float variationR{0.0f};
    float gainVarDbL{0.0f}; // Level-Offset L in dB → Stereo-Image
    float gainVarDbR{0.0f}; // Level-Offset R in dB → Stereo-Image
    float asymVarL{0.0f};   // Asymmetrie → 2nd Harmonics (Charakter L)
    float asymVarR{0.0f};   // Asymmetrie → 2nd Harmonics (Charakter R)

    // ── Console-Mode Smoothing (One-Pole, ~10 ms) ────────────────────────────
    float smoothedSag{1.0f};
    float smoothedMorphMod{0.0f};

    // ── Preamp / Pan Smoother (per-Sample, OS-Rate) ──────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothPreamp;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothPanL;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothPanR;
    // ── Binaural Pan (Woodworth-ITD + ILD) ───────────────────────────────────────
    // Max ITD @ 44.1 kHz: ~29 Samples  →  64 Samples Puffer reicht
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> binDelayL, binDelayR;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> binDelaySmL, binDelaySmR;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> binGainSmL, binGainSmR;    // ── Sub Left: 2nd-order Butterworth Crossover @ 77 Hz ────────────────────────
    // LPF → Sub des linken Kanals; HPF → Hochanteil des rechten Kanals
    juce::dsp::IIR::Filter<float> subLpfL, subHpfR;    // ── Shared parameter block (cross-plugin Console communication) ──────────
    SharedParameterBlock shmBlock;
    int shmChannel{-1};
    juce::String channelName{"CH"};

    // ── Hilfsfunktionen ──────────────────────────────────────────────────────
    /** Berechnet RMS eines Mono-Blocks (RT-safe). */
    static float computeRMS(const float *data, int numSamples) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SteinbachChanelStripAudioProcessor)
};
