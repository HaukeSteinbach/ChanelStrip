#include "EQProcessor.h"
#include <cmath>

EQProcessor::EQProcessor() = default;

void EQProcessor::prepare(double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = 4096;
    spec.numChannels      = 1; // jede Instanz verarbeitet Mono

    for (int ch = 0; ch < 2; ++ch)
    {
        hpf      [ch].prepare(spec);
        lowShelf [ch].prepare(spec);
        midBell  [ch].prepare(spec);
        highShelf[ch].prepare(spec);
    }

    paramsDirty = true;
    rebuildCoefficients();
}

void EQProcessor::setParameters(bool  hpfEnabled,
                                 float lowShelfFreqHz, float lowShelfGainDb,
                                 float midFreqHz,      float midGainDb, float midQ,
                                 float highShelfFreqHz,float highShelfGainDb)
{
    // Epsilon-Vergleich vermeiden: immer dirty setzen wenn aufgerufen.
    // Koeffizientenberechnung prüft selbst ob ein Rebuild nötig ist (einmal pro Block).
    const bool changed =
        (hpfEnabled      != pHpfEnabled)  ||
        std::abs(lowShelfFreqHz  - pLsFreq)  > 0.5f ||
        std::abs(lowShelfGainDb  - pLsGain)  > 0.05f ||
        std::abs(midFreqHz       - pMidFreq) > 0.5f ||
        std::abs(midGainDb       - pMidGain) > 0.05f ||
        std::abs(midQ            - pMidQ)    > 0.001f ||
        std::abs(highShelfFreqHz - pHsFreq)  > 0.5f ||
        std::abs(highShelfGainDb - pHsGain)  > 0.05f;

    if (changed)
    {
        pHpfEnabled = hpfEnabled;
        pLsFreq     = lowShelfFreqHz;  pLsGain  = lowShelfGainDb;
        pMidFreq    = midFreqHz;       pMidGain = midGainDb;      pMidQ = midQ;
        pHsFreq     = highShelfFreqHz; pHsGain  = highShelfGainDb;
        paramsDirty = true;
    }
}

void EQProcessor::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (paramsDirty)
        rebuildCoefficients();

    const int numCh = std::min(buffer.getNumChannels(), 2);

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer(ch);

        if (pHpfEnabled)
        {
            for (int n = 0; n < numSamples; ++n)
                data[n] = hpf[ch].processSample(data[n]);
        }

        for (int n = 0; n < numSamples; ++n)
            data[n] = lowShelf[ch].processSample(data[n]);

        for (int n = 0; n < numSamples; ++n)
            data[n] = midBell[ch].processSample(data[n]);

        for (int n = 0; n < numSamples; ++n)
            data[n] = highShelf[ch].processSample(data[n]);
    }
}

// ── Koeffizientenberechnung (außerhalb des Sample-Loops) ─────────────────────

void EQProcessor::rebuildCoefficients()
{
    const double sr = sampleRate;

    // HPF 40 Hz, Butterworth 2. Ordnung
    *hpf[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 40.0f, 0.7071f);
    *hpf[1].coefficients = *hpf[0].coefficients;

    // Low Shelf (sampleRate=double, freq/Q/gain=float)
    *lowShelf[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sr, pLsFreq, 0.7071f, juce::Decibels::decibelsToGain(pLsGain));
    *lowShelf[1].coefficients = *lowShelf[0].coefficients;

    // Mid Bell (Peak EQ)
    *midBell[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sr, pMidFreq, pMidQ, juce::Decibels::decibelsToGain(pMidGain));
    *midBell[1].coefficients = *midBell[0].coefficients;

    // High Shelf
    *highShelf[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sr, pHsFreq, 0.7071f, juce::Decibels::decibelsToGain(pHsGain));
    *highShelf[1].coefficients = *highShelf[0].coefficients;

    paramsDirty = false;
}

void EQProcessor::resetFilters()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        hpf      [ch].reset();
        lowShelf [ch].reset();
        midBell  [ch].reset();
        highShelf[ch].reset();
    }
}
