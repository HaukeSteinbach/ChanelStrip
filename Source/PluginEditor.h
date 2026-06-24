#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SteinbachChanelStripAudioProcessorEditor
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:
    explicit SteinbachChanelStripAudioProcessorEditor(SteinbachChanelStripAudioProcessor&);
    ~SteinbachChanelStripAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS        = juce::AudioProcessorValueTreeState;
    using SliderAttach = APVTS::SliderAttachment;
    using ButtonAttach = APVTS::ButtonAttachment;
    using ComboAttach  = APVTS::ComboBoxAttachment;

    SteinbachChanelStripAudioProcessor& audioProcessor;

    // ── EQ ───────────────────────────────────────────────────────────────────
    juce::Slider eqLowKnob, eqMidKnob, eqHighKnob;
    std::unique_ptr<SliderAttach> eqLowAttach, eqMidAttach, eqHighAttach;
    juce::ToggleButton hpfButton;
    std::unique_ptr<ButtonAttach> hpfAttach;

    // ── Routing ──────────────────────────────────────────────────────────────
    juce::Slider panKnob;
    std::unique_ptr<SliderAttach> panAttach;

    // ── Preamp ───────────────────────────────────────────────────────────────
    juce::Slider preampKnob, morphKnob;
    std::unique_ptr<SliderAttach> preampAttach, morphAttach;
    juce::ComboBox wavetableCombo;
    std::unique_ptr<ComboAttach> wavetableAttach;
    juce::ToggleButton lrLinkButton;
    std::unique_ptr<ButtonAttach> lrLinkAttach;

    // ── Console Mode ─────────────────────────────────────────────────────────
    juce::ToggleButton consoleModeButton;
    std::unique_ptr<ButtonAttach> consoleModeAttach;
    juce::ComboBox consoleGroupCombo;
    std::unique_ptr<ComboAttach> consoleGroupAttach;

    // ── Channel Name label (double-click to rename) ───────────────────────────
    juce::Label channelNameLabel;

    void initKnob(juce::Slider& s, const juce::String& tooltip);
    void timerCallback() override;  // polls channel name from processor

    std::unique_ptr<juce::LookAndFeel_V4> laf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SteinbachChanelStripAudioProcessorEditor)
};
