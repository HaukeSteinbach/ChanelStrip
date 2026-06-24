#pragma once

#include <JuceHeader.h>
#include "../SharedMemory/SharedParameterBlock.h"

/**
 * SteinbachConsole – stereo pass-through plugin.
 *
 * No audio processing; audio is passed through unchanged.
 * The Editor renders all active SharedParameterBlock channels as a mixing desk.
 */
class SteinbachConsoleAudioProcessor : public juce::AudioProcessor
{
public:
    SteinbachConsoleAudioProcessor();
    ~SteinbachConsoleAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool  acceptsMidi()  const override { return false; }
    bool  producesMidi() const override { return false; }
    bool  isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    SharedParameterBlock& getSharedBlock() noexcept { return sharedBlock; }

private:
    SharedParameterBlock sharedBlock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SteinbachConsoleAudioProcessor)
};
