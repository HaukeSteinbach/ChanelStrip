#pragma once

#include <JuceHeader.h>

/**
 * Zero-Latency EQ: HPF-40, Low Shelf, Mid Bell, High Shelf.
 *
 * RT-safe: keine Allokationen in process(). Koeffizienten werden
 * einmal pro Block (wenn dirty) neu berechnet – außerhalb der
 * Sample-Schleife. Alle Filter sind Minimum-Phase (IIR).
 */
class EQProcessor
{
public:
    EQProcessor();

    /** Muss in prepareToPlay aufgerufen werden (nicht RT). */
    void prepare(double sampleRate, int samplesPerBlock);

    /**
     * RT-safe. Ruft intern updateCoefficients() auf wenn Parameter dirty.
     * buffer muss L (0) und R (1) enthalten.
     */
    void process(juce::AudioBuffer<float>& buffer, int numSamples);

    /** Wird von PluginProcessor pro Block aufgerufen (nicht im Sample-Loop). */
    void setParameters(bool  hpfEnabled,
                       float lowShelfFreqHz,  float lowShelfGainDb,
                       float midFreqHz,       float midGainDb,  float midQ,
                       float highShelfFreqHz, float highShelfGainDb);

private:
    double sampleRate { 44100.0 };

    using Filter      = juce::dsp::IIR::Filter<float>;
    using Coeffs      = juce::dsp::IIR::Coefficients<float>;
    using CoeffsPtr   = juce::ReferenceCountedObjectPtr<Coeffs>;

    // Stereo filter chains: Index 0 = L, 1 = R
    Filter hpf        [2];
    Filter lowShelf   [2];
    Filter midBell    [2];
    Filter highShelf  [2];

    bool  paramsDirty      { true  };

    // Gecachte Parameterwerte (zum Vergleich, Änderungsdetektion)
    bool  pHpfEnabled      { false };
    float pLsFreq          { 80.0f   }, pLsGain  { 0.0f };
    float pMidFreq         { 1000.0f }, pMidGain { 0.0f }, pMidQ { 0.707f };
    float pHsFreq          { 8000.0f }, pHsGain  { 0.0f };

    void rebuildCoefficients();

    // Hilfsfunktion: Filter zurücksetzen (verhindert Denormals nach Parametersprung)
    void resetFilters();
};
