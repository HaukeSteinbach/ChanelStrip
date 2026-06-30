#include "EQProcessor.h"
#include <cmath>

EQProcessor::EQProcessor() = default;

void EQProcessor::prepare(double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = 4096;
    spec.numChannels = 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        hpf[ch].prepare(spec);
        lowShelf[ch].prepare(spec);
        midBell[ch].prepare(spec);
        highShelf[ch].prepare(spec);
    }

    paramsDirty = true;
    rebuildCoefficients();

    // Smoother initialisieren: starten bei aktuellen Werten, kein Ramp beim Start
    smLow.reset(sr, 0.050);
    smLow.setCurrentAndTargetValue(pLowGain);
    smMid.reset(sr, 0.050);
    smMid.setCurrentAndTargetValue(pMidGain);
    smHigh.reset(sr, 0.050);
    smHigh.setCurrentAndTargetValue(pHighGain);
}

void EQProcessor::setParameters(float lowGainDb, float midGainDb, float highGainDb,
                                bool hpfEnabled)
{
    if (hpfEnabled != pHpfEnabled)
    {
        pHpfEnabled = hpfEnabled;
        paramsDirty = true;
    }
    // Smoother-Targets: Koeffizienten werden über 50 ms sanft übergeblendet
    smLow.setTargetValue(lowGainDb);
    smMid.setTargetValue(midGainDb);
    smHigh.setTargetValue(highGainDb);
}

void EQProcessor::process(juce::AudioBuffer<float> &buffer, int numSamples)
{
    if (smLow.isSmoothing() || smMid.isSmoothing() || smHigh.isSmoothing() || paramsDirty)
    {
        smLow.skip(numSamples);
        smMid.skip(numSamples);
        smHigh.skip(numSamples);
        pLowGain = smLow.getCurrentValue();
        pMidGain = smMid.getCurrentValue();
        pHighGain = smHigh.getCurrentValue();
        rebuildCoefficients();
    }

    const int numCh = std::min(buffer.getNumChannels(), 2);

    for (int ch = 0; ch < numCh; ++ch)
    {
        float *data = buffer.getWritePointer(ch);

        if (pHpfEnabled)
            for (int n = 0; n < numSamples; ++n)
                data[n] = hpf[ch].processSample(data[n]);

        for (int n = 0; n < numSamples; ++n)
            data[n] = lowShelf[ch].processSample(data[n]);

        for (int n = 0; n < numSamples; ++n)
            data[n] = midBell[ch].processSample(data[n]);

        for (int n = 0; n < numSamples; ++n)
            data[n] = highShelf[ch].processSample(data[n]);
    }
}

void EQProcessor::rebuildCoefficients()
{
    const double sr = sampleRate;

    *hpf[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
        sr, 40.0f, 0.7071f);
    *hpf[1].coefficients = *hpf[0].coefficients;

    *lowShelf[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sr, 100.0f, 0.7071f, juce::Decibels::decibelsToGain(pLowGain));
    *lowShelf[1].coefficients = *lowShelf[0].coefficients;

    // Q sinkt linear mit dem Betrag des Gains: 1.0 bei 0 dB → 0.75 bei ±12 dB
    const float midQ = 1.0f - std::abs(pMidGain) * (0.25f / 12.0f);

    *midBell[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sr, 1000.0f, midQ, juce::Decibels::decibelsToGain(pMidGain));
    *midBell[1].coefficients = *midBell[0].coefficients;

    *highShelf[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sr, 10000.0f, 0.7071f, juce::Decibels::decibelsToGain(pHighGain));
    *highShelf[1].coefficients = *highShelf[0].coefficients;

    paramsDirty = false;
}
