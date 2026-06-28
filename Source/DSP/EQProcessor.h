#pragma once

#include <JuceHeader.h>

/**
 * Zero-Latency EQ: HPF-40, Low Shelf (100 Hz), Mid Bell (1 kHz), High Shelf (10 kHz).
 *
 * Nur 3 Gain-Parameter; Frequenzen sind fest.
 * Kubische Antwortkurve wird vom Aufrufer (via NormalisableRange) vorberechnet.
 * RT-safe: keine Allokationen in process().
 */
class EQProcessor
{
public:
    EQProcessor();

    void prepare(double sampleRate, int samplesPerBlock);

    /**
     * RT-safe. Ruft intern rebuildCoefficients() auf wenn dirty.
     */
    void process(juce::AudioBuffer<float> &buffer, int numSamples);

    /** Pro Block aufrufen – setzt dirty-Flag bei Parameteränderung. */
    void setParameters(float lowGainDb, float midGainDb, float highGainDb,
                       bool hpfEnabled);

private:
    double sampleRate{44100.0};

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    using CoeffsPtr = juce::ReferenceCountedObjectPtr<Coeffs>;

    Filter hpf[2];
    Filter lowShelf[2];
    Filter midBell[2];
    Filter highShelf[2];

    bool paramsDirty{true};
    bool pHpfEnabled{true};
    float pLowGain{0.0f};
    float pMidGain{0.0f};
    float pHighGain{0.0f};

    // Smoother: Gain-Werte rampen über 50 ms → keine Koeffizienten-Sprünge
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smLow, smMid, smHigh;

    void rebuildCoefficients();
};
