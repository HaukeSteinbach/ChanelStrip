#pragma once

#include <JuceHeader.h>
#include "ConsoleProcessor.h"

class SteinbachConsoleEditor : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit SteinbachConsoleEditor(SteinbachConsoleAudioProcessor&);
    ~SteinbachConsoleEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildChannelStrips();

    SteinbachConsoleAudioProcessor& proc;

    // ── Per-channel component (inner class) ───────────────────────────────────
    class ChannelStrip;
    juce::OwnedArray<ChannelStrip> strips;

    juce::Viewport viewport;
    juce::Component contentArea;

    std::unique_ptr<juce::LookAndFeel_V4> laf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SteinbachConsoleEditor)
};
