#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// ── PanKnob: Slider mit Rechtsklick-Menü (Binaural Pan) ───────────────────────────
class PanKnob : public juce::Slider
{
public:
    std::function<void()> onRightClick;
    void mouseDown(const juce::MouseEvent &e) override
    {
        if (e.mods.isRightButtonDown() && onRightClick)
        {
            onRightClick();
            return;
        }
        juce::Slider::mouseDown(e);
    }
};

class SteinbachChanelStripAudioProcessorEditor
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:
    explicit SteinbachChanelStripAudioProcessorEditor(SteinbachChanelStripAudioProcessor &);
    ~SteinbachChanelStripAudioProcessorEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttach = APVTS::SliderAttachment;
    using ButtonAttach = APVTS::ButtonAttachment;
    using ComboAttach = APVTS::ComboBoxAttachment;

    SteinbachChanelStripAudioProcessor &audioProcessor;

    // ── EQ ───────────────────────────────────────────────────────────────────
    juce::Slider eqLowKnob, eqMidKnob, eqHighKnob;
    std::unique_ptr<SliderAttach> eqLowAttach, eqMidAttach, eqHighAttach;
    juce::ToggleButton hpfButton;
    std::unique_ptr<ButtonAttach> hpfAttach;

    // ── Routing ──────────────────────────────────────────────────────────────
    PanKnob panKnob; // Rechtsklick → Binaural Pan ein/aus
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
    // ── Output Clipper ────────────────────────────────────────────────
    juce::ToggleButton clipperModeButton;
    std::unique_ptr<ButtonAttach> clipperModeAttach;
    // ── Channel Name label (double-click to rename) ───────────────────────────
    juce::Label channelNameLabel;

    void initKnob(juce::Slider &s, const juce::String &tooltip);
    void timerCallback() override; // polls channel name from processor
    void showPanContextMenu();     // Rechtsklick auf PanKnob

    std::unique_ptr<juce::LookAndFeel_V4> laf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SteinbachChanelStripAudioProcessorEditor)
};
