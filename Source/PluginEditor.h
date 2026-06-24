#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

/**
 * Basic Generic-UI – ersetzt durch Custom-GUI in einem späteren Schritt.
 * Erlaubt sofortiges Testen aller Parameter im Host ohne eigene Widgets.
 */
class SteinbachChanelStripAudioProcessorEditor
    : public juce::AudioProcessorEditor
{
public:
    explicit SteinbachChanelStripAudioProcessorEditor(SteinbachChanelStripAudioProcessor&);
    ~SteinbachChanelStripAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SteinbachChanelStripAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SteinbachChanelStripAudioProcessorEditor)
};
