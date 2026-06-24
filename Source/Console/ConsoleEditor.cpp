#include "ConsoleEditor.h"

namespace {
    const juce::Colour kBG      { 0xff1b1e25u };
    const juce::Colour kPanel   { 0xff242830u };
    const juce::Colour kBorder  { 0xff3a4058u };
    const juce::Colour kKnobBG  { 0xff131519u };
    const juce::Colour kAccent  { 0xff4e7ec4u };
    const juce::Colour kText    { 0xffeff2f8u };
    const juce::Colour kSubText { 0xff8290a8u };

    constexpr int kHdrH   = 36;
    constexpr int kChW    = 106;  // column width (inc. 14px left name strip)
    constexpr int kConH   = 430;  // content height
    constexpr int kGap    = 6;
    constexpr int kStrip  = 14;   // vertical name strip width

    constexpr int kGainSz = 54;
    constexpr int kSmSz   = 42;   // pan / morph
    constexpr int kEqSz   = 27;   // 3 eq knobs

    // Content x starts after the vertical name strip
    int cx(int sz) { return kStrip + (kChW - kStrip - sz) / 2; }
}

namespace {
class ConsoleLAF : public juce::LookAndFeel_V4
{
public:
    ConsoleLAF()
    {
        setColour(juce::Label::textColourId,            kText);
        setColour(juce::Label::backgroundColourId,      juce::Colours::transparentBlack);
        setColour(juce::TextEditor::backgroundColourId, kPanel);
        setColour(juce::TextEditor::textColourId,       kText);
        setColour(juce::TextEditor::outlineColourId,    kAccent);
        setColour(juce::ScrollBar::thumbColourId,       kBorder);
        setColour(juce::TextButton::buttonColourId,     kKnobBG);
        setColour(juce::TextButton::textColourOffId,    kSubText);
        setColour(juce::TextButton::textColourOnId,     kText);
        setColour(juce::PopupMenu::backgroundColourId,  kPanel);
        setColour(juce::PopupMenu::textColourId,        kText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha(0.3f));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float startAngle, float endAngle, juce::Slider&) override
    {
        const float cx = x+w*0.5f, cy = y+h*0.5f, r = std::min(w,h)*0.5f-1.5f;
        g.setColour(kKnobBG); g.fillEllipse(cx-r,cy-r,r*2,r*2);
        g.setColour(kBorder); g.drawEllipse(cx-r,cy-r,r*2,r*2,0.8f);
        juce::Path track;
        track.addArc(cx-r*0.72f,cy-r*0.72f,r*1.44f,r*1.44f,startAngle,endAngle,true);
        g.setColour(kAccent.withAlpha(0.2f));
        g.strokePath(track, juce::PathStrokeType(1.8f));
        juce::Path val;
        val.addArc(cx-r*0.72f,cy-r*0.72f,r*1.44f,r*1.44f,
                   startAngle, startAngle+sliderPos*(endAngle-startAngle), true);
        g.setColour(kAccent); g.strokePath(val, juce::PathStrokeType(2.0f));
        const float ang = startAngle+sliderPos*(endAngle-startAngle)-juce::MathConstants<float>::halfPi;
        juce::Path line;
        line.startNewSubPath(cx+std::cos(ang)*r*0.16f, cy+std::sin(ang)*r*0.16f);
        line.lineTo(cx+std::cos(ang)*r*0.68f,          cy+std::sin(ang)*r*0.68f);
        g.setColour(kText);
        g.strokePath(line, juce::PathStrokeType(1.3f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& btn,
        const juce::Colour&, bool hl, bool) override
    {
        const auto b = btn.getLocalBounds().toFloat().reduced(0.5f);
        const bool on = btn.getToggleState();
        g.setColour(on ? kAccent.withAlpha(0.35f) : kKnobBG);
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(on ? kAccent : (hl ? kBorder.brighter(0.4f) : kBorder));
        g.drawRoundedRectangle(b, 3.0f, 0.7f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& btn, bool, bool) override
    {
        g.setFont(juce::Font(8.5f));
        g.setColour(btn.getToggleState() ? kText : kSubText);
        g.drawText(btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred, false);
    }
};
} // anon

// ── ChannelStrip ──────────────────────────────────────────────────────────────
class SteinbachConsoleEditor::ChannelStrip : public juce::Component,
                                             public juce::Label::Listener
{
public:
    ChannelStrip(int idx, SharedParameterBlock& block)
        : channelIdx(idx), sharedBlock(block)
    {
        nameLabel.setEditable(false, true);
        nameLabel.setJustificationType(juce::Justification::centred);
        nameLabel.setFont(juce::Font(9.5f, juce::Font::bold));
        nameLabel.setColour(juce::Label::textColourId, kText);
        nameLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        nameLabel.addListener(this);
        addAndMakeVisible(nameLabel);

        resetBtn.setButtonText("R");
        resetBtn.setTooltip("Release Console control");
        resetBtn.onClick = [this]{
            if (auto* ch = sharedBlock.get(channelIdx))
                ch->hasOverride.store(0, std::memory_order_release);
            repaint();
        };
        addAndMakeVisible(resetBtn);

        initKnob(gainKnob,  -18.0, 18.0, [this]{ onControl(); });
        initKnob(panKnob,   -1.0,  1.0,  [this]{ onControl(); });
        initKnob(eqLow,     -12.0, 12.0, [this]{ onControl(); });
        initKnob(eqMid,     -12.0, 12.0, [this]{ onControl(); });
        initKnob(eqHigh,    -12.0, 12.0, [this]{ onControl(); });
        initKnob(morphKnob,  0.0,  1.0,  [this]{ onControl(); });
        gainKnob .setDoubleClickReturnValue(true, 0.0);
        panKnob  .setDoubleClickReturnValue(true, 0.0);
        eqLow    .setDoubleClickReturnValue(true, 0.0);
        eqMid    .setDoubleClickReturnValue(true, 0.0);
        eqHigh   .setDoubleClickReturnValue(true, 0.0);
        morphKnob.setDoubleClickReturnValue(true, 0.0);

        hpfBtn.setButtonText("HPF"); hpfBtn.setClickingTogglesState(true);
        hpfBtn.onClick = [this]{ onControl(); };
        addAndMakeVisible(hpfBtn);

        const char* cl[] = { "CLN","WRM","SAT" };
        for (int i=0;i<3;++i) {
            charBtns[i].setButtonText(cl[i]);
            charBtns[i].setClickingTogglesState(true);
            charBtns[i].setRadioGroupId(300+channelIdx, juce::dontSendNotification);
            charBtns[i].onClick = [this]{ onControl(); };
            addAndMakeVisible(charBtns[i]);
        }
        charBtns[0].setToggleState(true, juce::dontSendNotification);
    }

    ~ChannelStrip() override { nameLabel.removeListener(this); }

    void paint(juce::Graphics& g) override
    {
        const bool ov = isOverrideActive();

        // Panel
        const auto b = getLocalBounds().reduced(2).toFloat();
        g.setColour(ov ? kPanel.brighter(0.07f) : kPanel);
        g.fillRoundedRectangle(b, 4.0f);
        g.setColour(ov ? kAccent.withAlpha(0.75f) : kBorder);
        g.drawRoundedRectangle(b, 4.0f, ov ? 1.4f : 0.7f);

        // Vertical name strip (left edge, spanning gain+pan area)
        {
            const juce::String name = nameLabel.getText();
            if (name.isNotEmpty() && gainLY > 0 && morphLY > 0)
            {
                const float spanTop = (float)(gainLY);
                const float spanBot = (float)(morphLY + kSmSz);
                const float midY    = (spanTop + spanBot) * 0.5f;
                const float stripCX = 6.5f;
                const float textW   = spanBot - spanTop;

                juce::Graphics::ScopedSaveState ss(g);
                g.addTransform(juce::AffineTransform::rotation(
                    -juce::MathConstants<float>::halfPi, stripCX, midY));
                g.setColour(kSubText.withAlpha(0.6f));
                g.setFont(juce::Font(10.5f, juce::Font::bold));
                g.drawText(name,
                    (int)(stripCX - textW*0.5f), (int)(midY - 6.0f),
                    (int)textW, 12,
                    juce::Justification::centred, true);
            }
        }

        // Thin separator between name strip and content
        g.setColour(kBorder);
        g.drawLine((float)kStrip, 4.0f, (float)kStrip, getHeight()-4.0f, 0.6f);

        // Section labels (in content area)
        auto lbl = [&](const juce::String& t, int ly){
            g.setColour(kSubText); g.setFont(juce::Font(7.5f));
            g.drawText(t, kStrip, ly, getWidth()-kStrip-2, 11, juce::Justification::centred, false);
        };
        lbl("GAIN",      gainLY);
        lbl("PAN",       panLY);
        lbl("EQ",        eqLY);
        lbl("CHARACTER", morphLY);

        // EQ sub-labels
        g.setFont(juce::Font(7.0f)); g.setColour(kSubText);
        const int ex = kStrip+2;
        g.drawText("LO",  ex,              eqSubLY, kEqSz, 10, juce::Justification::centred, false);
        g.drawText("MID", ex+kEqSz+2,      eqSubLY, kEqSz, 10, juce::Justification::centred, false);
        g.drawText("HI",  ex+(kEqSz+2)*2,  eqSubLY, kEqSz, 10, juce::Justification::centred, false);

        // Override dot
        const float cr = 4.5f;
        g.setColour(ov ? kAccent : kBorder);
        g.fillEllipse(getWidth()-22.0f-cr, nameY+9.0f-cr, cr*2.0f, cr*2.0f);
    }

    void resized() override
    {
        int y = 5;
        nameY = y;
        nameLabel.setBounds(kStrip+2, y, getWidth()-kStrip-24, 18);
        resetBtn .setBounds(getWidth()-20,  y+1, 17, 16);
        y += 24;

        gainLY = y; y += 12;
        gainKnob.setBounds(cx(kGainSz), y, kGainSz, kGainSz); y += kGainSz+4;

        panLY = y; y += 12;
        panKnob.setBounds(cx(kSmSz), y, kSmSz, kSmSz); y += kSmSz+4;

        eqLY = y; y += 12;
        const int ex = kStrip+2;
        eqLow .setBounds(ex,             y, kEqSz, kEqSz);
        eqMid .setBounds(ex+kEqSz+2,     y, kEqSz, kEqSz);
        eqHigh.setBounds(ex+(kEqSz+2)*2, y, kEqSz, kEqSz);
        y += kEqSz;
        eqSubLY = y; y += 11;

        const int cw = (getWidth()-kStrip-6-28-3)/3;
        hpfBtn    .setBounds(kStrip+2,       y, 28, 18);
        charBtns[0].setBounds(kStrip+32,     y, cw, 18);
        charBtns[1].setBounds(kStrip+32+cw+2,y, cw, 18);
        charBtns[2].setBounds(kStrip+32+(cw+2)*2, y, cw, 18);
        y += 24;

        morphLY = y; y += 12;
        morphKnob.setBounds(cx(kSmSz), y, kSmSz, kSmSz);
    }

    int getChannelIdx() const noexcept { return channelIdx; }

    void refreshFromSharedBlock()
    {
        auto* ch = sharedBlock.get(channelIdx);
        if (!ch || !ch->active.load(std::memory_order_relaxed)) return;

        char buf[32]={};
        if (sharedBlock.readName(channelIdx, buf, sizeof(buf)) && buf[0]!='\0')
        {
            juce::String n(buf);
            if (n != nameLabel.getText())
                nameLabel.setText(n, juce::dontSendNotification);
        }

        if (isOverrideActive()) { repaint(); return; }

        suspendCallbacks = true;
        gainKnob .setValue(ch->preampDb .load(std::memory_order_relaxed), juce::dontSendNotification);
        panKnob  .setValue(ch->pan      .load(std::memory_order_relaxed), juce::dontSendNotification);
        eqLow    .setValue(ch->eqLow    .load(std::memory_order_relaxed), juce::dontSendNotification);
        eqMid    .setValue(ch->eqMid    .load(std::memory_order_relaxed), juce::dontSendNotification);
        eqHigh   .setValue(ch->eqHigh   .load(std::memory_order_relaxed), juce::dontSendNotification);
        morphKnob.setValue(ch->morph    .load(std::memory_order_relaxed), juce::dontSendNotification);
        const int wt = juce::jlimit(0,2,(int)ch->wavetable.load(std::memory_order_relaxed));
        charBtns[wt].setToggleState(true, juce::dontSendNotification);
        hpfBtn.setToggleState(ch->hpfEnabled.load(std::memory_order_relaxed)!=0, juce::dontSendNotification);
        suspendCallbacks = false;
        repaint();
    }

private:
    int channelIdx; SharedParameterBlock& sharedBlock; bool suspendCallbacks=false;
    int nameY=0, gainLY=0, panLY=0, eqLY=0, eqSubLY=0, morphLY=0;

    juce::Label      nameLabel;
    juce::TextButton resetBtn;
    juce::Slider     gainKnob, panKnob, eqLow, eqMid, eqHigh, morphKnob;
    juce::TextButton hpfBtn;
    juce::TextButton charBtns[3];

    bool isOverrideActive() const noexcept
    {
        const auto* ch = sharedBlock.get(channelIdx);
        return ch && ch->hasOverride.load(std::memory_order_relaxed)!=0;
    }

    template<typename Fn>
    void initKnob(juce::Slider& s, double lo, double hi, Fn&& fn)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setRange(lo, hi); s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        s.onValueChange = std::forward<Fn>(fn);
        addAndMakeVisible(s);
    }

    void onControl()
    {
        if (suspendCallbacks) return;
        auto* ch = sharedBlock.get(channelIdx);
        if (!ch) return;
        ch->ovPreampDb  .store((float)gainKnob .getValue(), std::memory_order_relaxed);
        ch->ovPan       .store((float)panKnob  .getValue(), std::memory_order_relaxed);
        ch->ovEqLow     .store((float)eqLow    .getValue(), std::memory_order_relaxed);
        ch->ovEqMid     .store((float)eqMid    .getValue(), std::memory_order_relaxed);
        ch->ovEqHigh    .store((float)eqHigh   .getValue(), std::memory_order_relaxed);
        ch->ovMorph     .store((float)morphKnob.getValue(), std::memory_order_relaxed);
        ch->ovHpfEnabled.store(hpfBtn.getToggleState()?1u:0u, std::memory_order_relaxed);
        int wt=0;
        for(int i=0;i<3;++i) if(charBtns[i].getToggleState()){wt=i;break;}
        ch->ovWavetable.store(wt, std::memory_order_relaxed);
        ch->hasOverride.store(1, std::memory_order_release);
        repaint();
    }

    void labelTextChanged(juce::Label* l) override
    {
        if (l==&nameLabel)
            sharedBlock.writeName(channelIdx, nameLabel.getText().toRawUTF8());
    }
};

// ── ConsoleEditor ─────────────────────────────────────────────────────────────
SteinbachConsoleEditor::SteinbachConsoleEditor(SteinbachConsoleAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    laf = std::make_unique<ConsoleLAF>();
    setLookAndFeel(laf.get());
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&contentArea, false);
    viewport.setScrollBarsShown(false, true);
    viewport.setScrollBarThickness(6);
    setSize(800, kHdrH + kConH);
    startTimerHz(10);
    rebuildChannelStrips();
}

SteinbachConsoleEditor::~SteinbachConsoleEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SteinbachConsoleEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBG);
    g.setColour(kPanel); g.fillRect(0,0,getWidth(),kHdrH);
    g.setColour(kBorder); g.drawLine(0.0f,(float)kHdrH,(float)getWidth(),(float)kHdrH,0.8f);

    g.setColour(kText); g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText("STEINBACH", 14, 2, 120, 16, juce::Justification::centredLeft, false);
    g.setColour(kSubText); g.setFont(juce::Font(9.0f));
    g.drawText("CONSOLE  v0.4", 14, 19, 140, 11, juce::Justification::centredLeft, false);

    const int n = proc.getSharedBlock().activeCount();
    g.setColour(kSubText); g.setFont(juce::Font(9.5f));
    g.drawText(juce::String(n)+(n==1?" channel":" channels"),
               getWidth()-130, 0, 118, kHdrH, juce::Justification::centredRight, false);

    if (n == 0) {
        g.setColour(kSubText); g.setFont(juce::Font(12.0f));
        g.drawText("Load 'Steinbach Chanel Strip' on your tracks to see channels here.",
                   20, kHdrH+kConH/2-12, getWidth()-40, 24,
                   juce::Justification::centred, false);
    }
}

void SteinbachConsoleEditor::resized()
{
    viewport.setBounds(0, kHdrH, getWidth(), getHeight()-kHdrH);
    const int totalW = std::max(getWidth(), (int)strips.size()*(kChW+kGap)+kGap);
    contentArea.setSize(totalW, getHeight()-kHdrH);
    for (int i=0; i<strips.size(); ++i)
        strips[i]->setBounds(kGap+i*(kChW+kGap), 4, kChW, contentArea.getHeight()-8);
}

void SteinbachConsoleEditor::timerCallback()
{
    rebuildChannelStrips();
    for (auto* s : strips) s->refreshFromSharedBlock();
    repaint();
}

void SteinbachConsoleEditor::rebuildChannelStrips()
{
    auto& shm = proc.getSharedBlock();
    if (!shm.isValid()) return;

    std::vector<int> active;
    shm.forEachActive([&](int idx, const SharedParameterBlock::Channel&) {
        active.push_back(idx);
    });
    std::sort(active.begin(), active.end());

    std::vector<int> current;
    for (auto* s : strips) current.push_back(s->getChannelIdx());
    if (active == current) return;

    contentArea.removeAllChildren();
    strips.clear();
    for (int idx : active) {
        auto* s = new ChannelStrip(idx, shm);
        s->setLookAndFeel(laf.get());
        strips.add(s);
        contentArea.addAndMakeVisible(s);
    }
    resized();
}
