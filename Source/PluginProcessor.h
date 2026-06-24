#pragma once

#include <JuceHeader.h>
#include "DSP/EQProcessor.h"
#include "DSP/WavetableManager.h"
#include "ConsoleMode/InstanceRegistry.h"

// ── Parameter-IDs ─────────────────────────────────────────────────────────────
namespace ParamID
{
    // EQ
    static constexpr auto HPF_ENABLED    = "hpf_enabled";
    static constexpr auto LS_FREQ        = "ls_freq";
    static constexpr auto LS_GAIN        = "ls_gain";
    static constexpr auto MID_FREQ       = "mid_freq";
    static constexpr auto MID_GAIN       = "mid_gain";
    static constexpr auto MID_Q          = "mid_q";
    static constexpr auto HS_FREQ        = "hs_freq";
    static constexpr auto HS_GAIN        = "hs_gain";

    // Routing
    static constexpr auto PAN            = "pan";

    // Preamp / Wavefolding
    static constexpr auto PREAMP_GAIN    = "preamp_gain";
    static constexpr auto MORPH_AMOUNT   = "morph_amount";   // Wavefolding-Intensität
    static constexpr auto WAVETABLE_IDX  = "wavetable_idx";
    static constexpr auto LR_LINK        = "lr_link";        // L→R Variation linken

    // Instanz-Variation
    static constexpr auto VARIATION_L    = "variation_l";    // ±-Wert, Anzeige only
    static constexpr auto VARIATION_R    = "variation_r";

    // Console Mode
    static constexpr auto CONSOLE_ENABLE = "console_enable";
    static constexpr auto CONSOLE_GROUP  = "console_group";
}

// ── Processor ─────────────────────────────────────────────────────────────────
class SteinbachChanelStripAudioProcessor : public juce::AudioProcessor
{
public:
    SteinbachChanelStripAudioProcessor();
    ~SteinbachChanelStripAudioProcessor() override;

    // ── AudioProcessor API ───────────────────────────────────────────────────
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool  acceptsMidi()  const override { return false; }
    bool  producesMidi() const override { return false; }
    bool  isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── Public State ─────────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** Lade eigene Wavetable (non-RT, z.B. aus FileChooser). */
    void loadCustomWavetable(const float* data, int numRows, int numCols);

private:
    // ── DSP ──────────────────────────────────────────────────────────────────
    EQProcessor      eqProcessor;
    WavetableManager wavetableManagerL;   // L-Kanal Variation
    WavetableManager wavetableManagerR;   // R-Kanal Variation

    // ── Console Mode ─────────────────────────────────────────────────────────
    int instanceSlot { -1 };

    // ── Instanz-Variation ────────────────────────────────────────────────────
    float variationL { 0.03f };
    float variationR { 0.03f };

    // ── Hilfsfunktionen ──────────────────────────────────────────────────────
    /** Berechnet RMS eines Mono-Blocks (RT-safe). */
    static float computeRMS(const float* data, int numSamples) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SteinbachChanelStripAudioProcessor)
};
