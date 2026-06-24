#include "EQProcessor.h"
#include <cmath>

EQProcessor::EQProcessor() = default;

void EQProcessor::prepare(double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = 4096;
    spec.numChannels      = 1;

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

void EQProcessor::setParameters(float lowGainDb, float midGainDb, float highGainDb,
                                 bool hpfEnabled)
{
    const bool changed =
        (hpfEnabled != pHpfEnabled)                      ||
        std::abs(lowGainDb  - pLowGain)  > 0.05f ||
        std::abs(midGainDb  - pMidGain)  > 0.05f ||
        std::abs(highGainDb - pHighGain) > 0.05f;

    if (changed)
    {
        pHpfEnabled = hpfEnabled;
        pLowGain    = lowGainDb;
        pMidGain    = midGainDb;
        pHighGain   = highGainDb;
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

    *midBell[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sr, 1000.0f, 1.0f, juce::Decibels::decibelsToGain(pMidGain));
    *midBell[1].coefficients = *midBell[0].coefficients;

    *highShelf[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sr, 10000.0f, 0.7071f, juce::Decibels::decibelsToGain(pHighGain));
    *highShelf[1].coefficients = *highShelf[0].coefficients;

    paramsDirty = false;
}
