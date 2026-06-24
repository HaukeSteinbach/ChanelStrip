#include "PluginEditor.h"

SteinbachChanelStripAudioProcessorEditor::SteinbachChanelStripAudioProcessorEditor(
    SteinbachChanelStripAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Minimale Fenstergröße – später durch Custom-GUI ersetzen
    setSize(600, 400);
}

SteinbachChanelStripAudioProcessorEditor::~SteinbachChanelStripAudioProcessorEditor() = default;

void SteinbachChanelStripAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawFittedText("Steinbach Chanel Strip\n[Generic UI – Custom GUI folgt]",
                     getLocalBounds(), juce::Justification::centred, 2);
}

void SteinbachChanelStripAudioProcessorEditor::resized() {}

juce::AudioProcessorEditor*
SteinbachChanelStripAudioProcessor::createEditor()
{
    // Generic Editor für alle Parameter (kein Custom-GUI nötig zum Testen)
    return new juce::GenericAudioProcessorEditor(*this);
}
