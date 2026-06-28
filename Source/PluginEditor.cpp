#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
// Colors
// ─────────────────────────────────────────────────────────────────────────────
static const juce::Colour kBG{0xff1b1e25};
static const juce::Colour kPanel{0xff242830};
static const juce::Colour kBorder{0xff3a4058};
static const juce::Colour kKnobBG{0xff131519};
static const juce::Colour kAccent{0xff4e7ec4};
static const juce::Colour kText{0xffeff2f8};
static const juce::Colour kSubText{0xff8290a8};

// ─────────────────────────────────────────────────────────────────────────────
// Layout
//   3 equal knob columns + right-controls column filling the remaining space
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int kW = 560;
static constexpr int kH = 480;
static constexpr int kHdrH = 36;
static constexpr int kMgn = 10;
static constexpr int kPanelX = kMgn;
static constexpr int kPanelW = kW - 2 * kMgn; // 540

static constexpr int kKnobSz = 88;
static constexpr int kKnobGap = 22;
static constexpr int kKnobSp = kKnobSz + kKnobGap; // 110
static constexpr int kKnobPadL = 24;
static constexpr int kKnobPadT = 30;
static constexpr int kPGap = 8;

static constexpr int kEqY = kHdrH + kMgn; // 46
static constexpr int kEqH = 154;
static constexpr int kRtgY = kEqY + kEqH + kPGap; // 208
static constexpr int kRtgH = 154;
static constexpr int kConY = kRtgY + kRtgH + kPGap; // 370
static constexpr int kConH = kH - kConY - kMgn;     // 100

static constexpr int kEqKnobY = kEqY + kKnobPadT;   // 76
static constexpr int kRtgKnobY = kRtgY + kKnobPadT; // 238

// Knob X positions (3 columns, evenly spaced from left)
static constexpr int kKX0 = kPanelX + kKnobPadL; // 34
static constexpr int kKX1 = kKX0 + kKnobSp;      // 144
static constexpr int kKX2 = kKX1 + kKnobSp;      // 254

// Right-controls zone (fills remaining panel width)
static constexpr int kRX = kKX2 + kKnobSz + 18;         // 360
static constexpr int kRW = kPanelX + kPanelW - kRX - 8; // 182

// ─────────────────────────────────────────────────────────────────────────────
// Custom LookAndFeel  –  flat, modern
// ─────────────────────────────────────────────────────────────────────────────
class PluginLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    // ── Rotary knob ──────────────────────────────────────────────────────────
    void drawRotarySlider(juce::Graphics &g, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider &) override
    {
        const float cx = x + w * 0.5f;
        const float cy = y + h * 0.5f;
        const float r = juce::jmin(w, h) * 0.5f - 3.0f;

        // Flat body
        g.setColour(kKnobBG);
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

        // Thin rim
        g.setColour(kBorder.withAlpha(1.0f));
        g.drawEllipse(cx - r + 0.5f, cy - r + 0.5f,
                      r * 2.0f - 1.0f, r * 2.0f - 1.0f, 0.9f);

        const float arcR = r - 5.0f;
        const float toAngle = startAngle + sliderPos * (endAngle - startAngle);

        // Track arc
        {
            juce::Path track;
            track.addArc(cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f,
                         startAngle, endAngle, true);
            g.setColour(kAccent.withAlpha(0.20f));
            g.strokePath(track, juce::PathStrokeType(2.2f,
                                                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Value arc
        if (toAngle > startAngle + 0.01f)
        {
            juce::Path arc;
            arc.addArc(cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f,
                       startAngle, toAngle, true);
            g.setColour(kAccent);
            g.strokePath(arc, juce::PathStrokeType(2.5f,
                                                   juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Indicator needle – thin, no dot
        const float sinA = std::sin(toAngle);
        const float cosA = std::cos(toAngle);
        g.setColour(juce::Colours::white.withAlpha(0.82f));
        g.drawLine(cx + r * 0.16f * sinA, cy - r * 0.16f * cosA,
                   cx + r * 0.70f * sinA, cy - r * 0.70f * cosA, 1.6f);
    }

    // ── Toggle button ────────────────────────────────────────────────────────
    void drawToggleButton(juce::Graphics &g, juce::ToggleButton &btn,
                          bool isHighlighted, bool) override
    {
        constexpr int bSz = 15;
        const int bY = (btn.getHeight() - bSz) / 2;
        const bool on = btn.getToggleState();
        const juce::Rectangle<float> box(2.0f, (float)bY, (float)bSz, (float)bSz);

        g.setColour(on ? kAccent.withAlpha(0.80f) : kKnobBG);
        g.fillRoundedRectangle(box, 3.0f);
        g.setColour(on ? kAccent : kBorder);
        g.drawRoundedRectangle(box, 3.0f, 1.0f);

        if (on)
        {
            g.setColour(juce::Colours::white);
            const float cx = box.getCentreX(), cy = box.getCentreY();
            juce::Path tick;
            tick.startNewSubPath(cx - 3.8f, cy + 0.2f);
            tick.lineTo(cx - 1.0f, cy + 3.5f);
            tick.lineTo(cx + 4.5f, cy - 3.4f);
            g.strokePath(tick, juce::PathStrokeType(1.5f,
                                                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour(isHighlighted ? kText : kText.withAlpha(0.82f));
        g.setFont(juce::Font(12.0f));
        g.drawText(btn.getButtonText(), bSz + 7, 0,
                   btn.getWidth() - bSz - 7, btn.getHeight(),
                   juce::Justification::centredLeft);
    }

    // ── Combo box ────────────────────────────────────────────────────────────
    void drawComboBox(juce::Graphics &g, int w, int h, bool,
                      int, int, int, int, juce::ComboBox &) override
    {
        const juce::Rectangle<float> r(0.5f, 0.5f, (float)(w - 1), (float)(h - 1));
        g.setColour(kKnobBG);
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(kBorder);
        g.drawRoundedRectangle(r, 4.0f, 1.0f);

        // Small arrow
        const float ax = (float)(w - 14);
        const float ay = (float)h * 0.5f - 1.0f;
        juce::Path arrow;
        arrow.addTriangle(ax - 3.5f, ay - 2.0f,
                          ax + 3.5f, ay - 2.0f,
                          ax, ay + 3.0f);
        g.setColour(kSubText.brighter(0.3f));
        g.fillPath(arrow);
    }

    void positionComboBoxText(juce::ComboBox &box, juce::Label &label) override
    {
        label.setBounds(9, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont(juce::Font(12.0f));
        label.setColour(juce::Label::textColourId, kText);
    }

    // ── Popup menu ───────────────────────────────────────────────────────────
    void drawPopupMenuBackground(juce::Graphics &g, int w, int h) override
    {
        g.fillAll(juce::Colour(0xff181b32));
        g.setColour(kBorder);
        g.drawRect(0, 0, w, h, 1);
    }

    void drawPopupMenuItem(juce::Graphics &g, const juce::Rectangle<int> &area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool, bool, const juce::String &text,
                           const juce::String &, const juce::Drawable *,
                           const juce::Colour *) override
    {
        if (isSeparator)
        {
            g.setColour(kBorder);
            g.drawLine((float)area.getX() + 4, (float)area.getCentreY(),
                       (float)area.getRight() - 4, (float)area.getCentreY(), 0.5f);
            return;
        }
        if (isHighlighted && isActive)
        {
            g.setColour(kAccent.withAlpha(0.25f));
            g.fillRoundedRectangle(area.reduced(3, 1).toFloat(), 3.0f);
        }
        g.setColour(isActive ? kText : kSubText);
        g.setFont(juce::Font(12.0f));
        g.drawText(text, area.withTrimmedLeft(10).withTrimmedRight(6),
                   juce::Justification::centredLeft);
    }

    int getPopupMenuBorderSize() override { return 1; }
};

// ─────────────────────────────────────────────────────────────────────────────
// showPanContextMenu
// ─────────────────────────────────────────────────────────────────────────────
void SteinbachChanelStripAudioProcessorEditor::showPanContextMenu()
{
    const bool isOn = *audioProcessor.apvts.getRawParameterValue(ParamID::BINAURAL_PAN) > 0.5f;

    juce::PopupMenu menu;
    menu.addSectionHeader("Pan-Modus");
    menu.addItem(1, "Binaural Pan", true, isOn);

    menu.showMenuAsync({}, [this](int result)
                       {
        if (result == 1)
        {
            if (auto* p = audioProcessor.apvts.getParameter(ParamID::BINAURAL_PAN))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
                p->endChangeGesture();
            }
        } });
}

// ─────────────────────────────────────────────────────────────────────────────
// initKnob
// ─────────────────────────────────────────────────────────────────────────────
void SteinbachChanelStripAudioProcessorEditor::initKnob(
    juce::Slider &s, const juce::String &tooltip)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setTooltip(tooltip);
    addAndMakeVisible(s);
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────
SteinbachChanelStripAudioProcessorEditor::SteinbachChanelStripAudioProcessorEditor(
    SteinbachChanelStripAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      laf(std::make_unique<PluginLookAndFeel>())
{
    setLookAndFeel(laf.get());
    auto &apvts = p.apvts;

    initKnob(eqLowKnob, "Low Shelf 100 Hz +/-12 dB");
    initKnob(eqMidKnob, "Mid Bell 1 kHz +/-12 dB");
    initKnob(eqHighKnob, "High Shelf 10 kHz +/-12 dB");
    eqLowAttach = std::make_unique<SliderAttach>(apvts, ParamID::EQ_LOW, eqLowKnob);
    eqMidAttach = std::make_unique<SliderAttach>(apvts, ParamID::EQ_MID, eqMidKnob);
    eqHighAttach = std::make_unique<SliderAttach>(apvts, ParamID::EQ_HIGH, eqHighKnob);

    hpfButton.setButtonText("HPF 40");
    hpfButton.setClickingTogglesState(true);
    hpfAttach = std::make_unique<ButtonAttach>(apvts, ParamID::HPF_ENABLED, hpfButton);
    addAndMakeVisible(hpfButton);

    initKnob(panKnob, "Pan (L / R)  |  Rechtsklick: Binaural Pan");
    panKnob.onRightClick = [this]
    { showPanContextMenu(); };
    initKnob(preampKnob, "Preamp Gain +/-24 dB");
    initKnob(morphKnob, "Character");
    panAttach = std::make_unique<SliderAttach>(apvts, ParamID::PAN, panKnob);
    preampAttach = std::make_unique<SliderAttach>(apvts, ParamID::PREAMP_GAIN, preampKnob);
    morphAttach = std::make_unique<SliderAttach>(apvts, ParamID::MORPH_AMOUNT, morphKnob);

    wavetableCombo.addItem("Warm (Neve Transformer)", 1);
    wavetableCombo.addItem("Medium (Tube Class-A)", 2);
    wavetableCombo.addItem("Hot (Wavefolder)", 3);
    wavetableAttach = std::make_unique<ComboAttach>(apvts, ParamID::WAVETABLE_IDX, wavetableCombo);
    addAndMakeVisible(wavetableCombo);

    lrLinkButton.setButtonText("L/R Link");
    lrLinkButton.setClickingTogglesState(true);
    lrLinkAttach = std::make_unique<ButtonAttach>(apvts, ParamID::LR_LINK, lrLinkButton);
    addAndMakeVisible(lrLinkButton);

    consoleModeButton.setButtonText("Console Mode");
    consoleModeButton.setClickingTogglesState(true);
    consoleModeAttach = std::make_unique<ButtonAttach>(apvts, ParamID::CONSOLE_ENABLE, consoleModeButton);
    addAndMakeVisible(consoleModeButton);

    consoleGroupCombo.addItem("Group 1", 1);
    consoleGroupCombo.addItem("Group 2", 2);
    consoleGroupCombo.addItem("Group 3", 3);
    consoleGroupCombo.addItem("Group 4", 4);
    consoleGroupAttach = std::make_unique<ComboAttach>(apvts, ParamID::CONSOLE_GROUP, consoleGroupCombo);
    addAndMakeVisible(consoleGroupCombo);

    clipperModeButton.setButtonText("Soft Clip");
    clipperModeButton.setClickingTogglesState(true);
    clipperModeButton.setTooltip("Output Clipper: Hard OFF / Neve Soft ON  (≤−4 dB, immer aktiv)");
    clipperModeAttach = std::make_unique<ButtonAttach>(apvts, ParamID::CLIPPER_MODE, clipperModeButton);
    addAndMakeVisible(clipperModeButton);

    // Channel name label (double-click to rename; name synced to SharedParameterBlock)
    channelNameLabel.setText(audioProcessor.getChannelDisplayName(), juce::dontSendNotification);
    channelNameLabel.setEditable(false, true);
    channelNameLabel.setJustificationType(juce::Justification::centred);
    channelNameLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    channelNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc0c2e8u));
    channelNameLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1a1d38u));
    channelNameLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xff252848u));
    channelNameLabel.onTextChange = [this]
    {
        audioProcessor.setChannelDisplayName(channelNameLabel.getText());
    };
    addAndMakeVisible(channelNameLabel);
    startTimerHz(2); // poll channel name 2x/sec (catches DAW renames)

    setSize(kW, kH);
}

SteinbachChanelStripAudioProcessorEditor::~SteinbachChanelStripAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SteinbachChanelStripAudioProcessorEditor::timerCallback()
{
    const juce::String current = audioProcessor.getChannelDisplayName();
    if (current != channelNameLabel.getText())
        channelNameLabel.setText(current, juce::dontSendNotification);
}

// ─────────────────────────────────────────────────────────────────────────────
// paint
// ─────────────────────────────────────────────────────────────────────────────
void SteinbachChanelStripAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(kBG);

    // ── Title (2-row: top=STEINBACH, bottom=CHANNEL STRIP v1.0) ──────────────────────
    g.setColour(kText);
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("STEINBACH", 14, 2, 140, 16, juce::Justification::centredLeft);
    g.setColour(kSubText.brighter(0.2f));
    g.setFont(juce::Font(10.0f));
    g.drawText("CHANNEL STRIP  v1.0", 14, 19, 180, 13, juce::Justification::centredLeft);

    // Thin divider
    g.setColour(kBorder);
    g.drawLine(0.0f, (float)kHdrH - 0.5f, (float)kW, (float)kHdrH - 0.5f, 0.8f);

    // ── Panels ───────────────────────────────────────────────────────────────
    auto drawPanel = [&](juce::Rectangle<int> ri, const juce::String &label)
    {
        // Very subtle top-to-bottom gradient
        juce::ColourGradient grad(
            kPanel.brighter(0.06f), (float)ri.getX(), (float)ri.getY(),
            kPanel, (float)ri.getX(), (float)ri.getBottom(),
            false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(ri.toFloat(), 7.0f);

        g.setColour(kBorder);
        g.drawRoundedRectangle(ri.toFloat().reduced(0.5f), 7.0f, 1.0f);

        if (label.isNotEmpty())
        {
            g.setColour(kSubText);
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText(label, ri.getX() + 14, ri.getY() + 9,
                       ri.getWidth() - 20, 12,
                       juce::Justification::centredLeft);
        }
    };

    drawPanel({kPanelX, kEqY, kPanelW, kEqH}, "EQ");
    drawPanel({kPanelX, kRtgY, kPanelW, kRtgH}, "ROUTING / PREAMP");
    drawPanel({kPanelX, kConY, kPanelW, kConH}, "ANALOG CONSOLE MODE");

    // ── Clipper-Abschnitt innerhalb des Console-Panels ───────────────────────
    // Vertikaler Separator
    g.setColour(kBorder.withAlpha(0.6f));
    g.drawLine(kPanelX + 372.0f, (float)(kConY + 18), kPanelX + 372.0f, (float)(kConY + kConH - 18), 0.7f);
    // Sub-Label
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.setColour(kSubText);
    g.drawText("CLIPPER", kPanelX + 382, kConY + 9, 140, 12, juce::Justification::centredLeft);

    // ── Subtle vertical divider between knobs and right controls ─────────────
    const int divX = kRX - 10;
    g.setColour(kBorder.withAlpha(0.6f));
    // EQ panel divider
    g.drawLine((float)divX, (float)(kEqY + 18), (float)divX, (float)(kEqY + kEqH - 18), 0.7f);
    // Routing panel divider
    g.drawLine((float)divX, (float)(kRtgY + 18), (float)divX, (float)(kRtgY + kRtgH - 18), 0.7f);

    // ── Knob labels ──────────────────────────────────────────────────────────
    g.setFont(juce::Font(10.0f));
    g.setColour(kSubText.brighter(0.2f));

    const int eqLblY = kEqKnobY + kKnobSz + 6;
    const int rtgLblY = kRtgKnobY + kKnobSz + 6;

    g.drawText("LOW", kKX0, eqLblY, kKnobSz, 14, juce::Justification::centred);
    g.drawText("MID", kKX1, eqLblY, kKnobSz, 14, juce::Justification::centred);
    g.drawText("HIGH", kKX2, eqLblY, kKnobSz, 14, juce::Justification::centred);

    g.drawText("PAN", kKX0, rtgLblY, kKnobSz, 14, juce::Justification::centred);
    g.drawText("GAIN", kKX1, rtgLblY, kKnobSz, 14, juce::Justification::centred);
    g.drawText("CHARACTER", kKX2 - 8, rtgLblY, kKnobSz + 16, 14, juce::Justification::centred);
}

// ─────────────────────────────────────────────────────────────────────────────
// resized
// ─────────────────────────────────────────────────────────────────────────────
void SteinbachChanelStripAudioProcessorEditor::resized()
{
    // EQ knobs
    eqLowKnob.setBounds(kKX0, kEqKnobY, kKnobSz, kKnobSz);
    eqMidKnob.setBounds(kKX1, kEqKnobY, kKnobSz, kKnobSz);
    eqHighKnob.setBounds(kKX2, kEqKnobY, kKnobSz, kKnobSz);

    // HPF – centered vertically with knobs, in right zone
    hpfButton.setBounds(kRX + (kRW - 90) / 2,
                        kEqKnobY + (kKnobSz - 26) / 2,
                        90, 26);

    // Routing knobs
    panKnob.setBounds(kKX0, kRtgKnobY, kKnobSz, kKnobSz);
    preampKnob.setBounds(kKX1, kRtgKnobY, kKnobSz, kKnobSz);
    morphKnob.setBounds(kKX2, kRtgKnobY, kKnobSz, kKnobSz);

    // Combo + L/R Link – fill right zone
    wavetableCombo.setBounds(kRX, kRtgKnobY + 16, kRW, 26);
    lrLinkButton.setBounds(kRX + 2, kRtgKnobY + 56, kRW, 24);

    // Console
    const int cy = kConY + (kConH - 26) / 2;
    consoleModeButton.setBounds(kPanelX + 16, cy, 140, 26);
    consoleGroupCombo.setBounds(kPanelX + 168, cy, 180, 26);

    // Clipper toggle – rechte Hälfte des Console-Panels
    clipperModeButton.setBounds(kPanelX + 382, cy, 148, 26);

    // Channel name – right half of header, no overlap with title
    channelNameLabel.setBounds(200, 5, kW - 210, 26);
}

juce::AudioProcessorEditor *
SteinbachChanelStripAudioProcessor::createEditor()
{
    return new SteinbachChanelStripAudioProcessorEditor(*this);
}
