#include "ConsoleProcessor.h"
#include "ConsoleEditor.h"

SteinbachConsoleAudioProcessor::SteinbachConsoleAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void SteinbachConsoleAudioProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/) {}

bool SteinbachConsoleAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void SteinbachConsoleAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/)
{
    // Pass-through: audio unchanged.
    juce::ignoreUnused(buffer);
}

juce::AudioProcessorEditor* SteinbachConsoleAudioProcessor::createEditor()
{
    return new SteinbachConsoleEditor(*this);
}

void SteinbachConsoleAudioProcessor::getStateInformation(juce::MemoryBlock& /*data*/) {}
void SteinbachConsoleAudioProcessor::setStateInformation(const void* /*data*/, int /*size*/) {}

// ── Plugin entry point ────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SteinbachConsoleAudioProcessor();
}
