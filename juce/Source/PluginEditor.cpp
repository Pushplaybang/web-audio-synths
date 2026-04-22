#include "PluginEditor.h"

namespace
{
    const auto kBgColour      = juce::Colour(0xff0a0a0c);
    const auto kSectionBg     = juce::Colour(0xff1a1a22);
    const auto kAccentColour  = juce::Colour(0xffff5722);
    const auto kTextColour    = juce::Colour(0xffcccccc);
    const auto kLabelColour   = juce::Colour(0xffff8a65);
    const auto kDimTextColour = juce::Colour(0xff888888);

    constexpr int kWidth  = 900;
    constexpr int kHeight = 700;
}

// ── Constructor ───────────────────────────────────────────────
Acid303Editor::Acid303Editor(Acid303Processor& p)
    : AudioProcessorEditor(p), processor_(p)
{
    setSize(kWidth, kHeight);

    // ── Title ─────────────────────────────────────────────────
    titleLabel_.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    titleLabel_.setColour(juce::Label::textColourId, kAccentColour);
    addAndMakeVisible(titleLabel_);

    subtitleLabel_.setFont(juce::FontOptions(12.0f));
    subtitleLabel_.setColour(juce::Label::textColourId, kDimTextColour);
    addAndMakeVisible(subtitleLabel_);

    // ── Section labels ────────────────────────────────────────
    for (auto* label : { &oscLabel_, &filterLabel_, &envLabel_, &lfoLabel_,
                         &distLabel_, &delayLabel_, &reverbLabel_, &seqLabel_ })
        setupSectionLabel(*label);

    // ── Oscillator waveform buttons ───────────────────────────
    auto& apvts = processor_.getAPVTS();

    sawBtn_.setClickingTogglesState(true);
    sqrBtn_.setClickingTogglesState(true);
    sawBtn_.setToggleState(true, juce::dontSendNotification);
    sawBtn_.setRadioGroupId(1);
    sqrBtn_.setRadioGroupId(1);
    addAndMakeVisible(sawBtn_);
    addAndMakeVisible(sqrBtn_);

    sawBtn_.onClick = [this, &apvts]
    {
        if (auto* param = apvts.getParameter("waveform"))
            param->setValueNotifyingHost(0.0f);
    };
    sqrBtn_.onClick = [this, &apvts]
    {
        if (auto* param = apvts.getParameter("waveform"))
            param->setValueNotifyingHost(1.0f);
    };

    // ── Knobs + attachments ───────────────────────────────────
    auto addKnob = [this](AcidKnob& knob)
    {
        addAndMakeVisible(knob);
        knob.setColour(juce::Slider::rotarySliderFillColourId, kAccentColour);
        knob.setColour(juce::Slider::textBoxTextColourId, kTextColour);
        knob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    };

    addKnob(tuningKnob_);
    addKnob(cutoffKnob_);
    addKnob(resoKnob_);
    addKnob(envModKnob_);
    addKnob(decayKnob_);
    addKnob(accentKnob_);
    addKnob(lfoRateKnob_);
    addKnob(lfoAmountKnob_);
    addKnob(distAmountKnob_);
    addKnob(delayTimeKnob_);
    addKnob(delayFbKnob_);
    addKnob(delayMixKnob_);
    addKnob(reverbDecayKnob_);
    addKnob(reverbMixKnob_);
    addKnob(tempoKnob_);

    // APVTS slider attachments
    tuningAttach_      = std::make_unique<SliderAttachment>(apvts, "tuning",        tuningKnob_);
    cutoffAttach_      = std::make_unique<SliderAttachment>(apvts, "cutoff",        cutoffKnob_);
    resoAttach_        = std::make_unique<SliderAttachment>(apvts, "resonance",     resoKnob_);
    envModAttach_      = std::make_unique<SliderAttachment>(apvts, "envMod",        envModKnob_);
    decayAttach_       = std::make_unique<SliderAttachment>(apvts, "decay",         decayKnob_);
    accentAttach_      = std::make_unique<SliderAttachment>(apvts, "accent",        accentKnob_);
    distAmountAttach_  = std::make_unique<SliderAttachment>(apvts, "distAmount",    distAmountKnob_);
    delayTimeAttach_   = std::make_unique<SliderAttachment>(apvts, "delayTime",     delayTimeKnob_);
    delayFbAttach_     = std::make_unique<SliderAttachment>(apvts, "delayFeedback", delayFbKnob_);
    delayMixAttach_    = std::make_unique<SliderAttachment>(apvts, "delayMix",      delayMixKnob_);
    reverbDecayAttach_ = std::make_unique<SliderAttachment>(apvts, "reverbDecay",   reverbDecayKnob_);
    reverbMixAttach_   = std::make_unique<SliderAttachment>(apvts, "reverbMix",     reverbMixKnob_);
    lfoRateAttach_     = std::make_unique<SliderAttachment>(apvts, "lfoRate",       lfoRateKnob_);
    lfoAmountAttach_   = std::make_unique<SliderAttachment>(apvts, "lfoAmount",     lfoAmountKnob_);
    tempoAttach_       = std::make_unique<SliderAttachment>(apvts, "tempo",         tempoKnob_);

    // ── Toggle buttons ────────────────────────────────────────
    addAndMakeVisible(distToggle_);
    addAndMakeVisible(delayToggle_);
    addAndMakeVisible(reverbToggle_);

    distOnAttach_   = std::make_unique<ButtonAttachment>(apvts, "distOn",   distToggle_);
    delayOnAttach_  = std::make_unique<ButtonAttachment>(apvts, "delayOn",  delayToggle_);
    reverbOnAttach_ = std::make_unique<ButtonAttachment>(apvts, "reverbOn", reverbToggle_);

    // ── LFO wave buttons ──────────────────────────────────────
    for (auto* btn : { &lfoSineBtn_, &lfoTriBtn_, &lfoSqrBtn_ })
    {
        btn->setClickingTogglesState(true);
        btn->setRadioGroupId(2);
        addAndMakeVisible(btn);
    }
    lfoSineBtn_.setToggleState(true, juce::dontSendNotification);

    lfoSineBtn_.onClick = [&apvts]
    {
        if (auto* p = apvts.getParameter("lfoWave"))
            p->setValueNotifyingHost(0.0f);
    };
    lfoTriBtn_.onClick = [&apvts]
    {
        if (auto* p = apvts.getParameter("lfoWave"))
            p->setValueNotifyingHost(0.5f);
    };
    lfoSqrBtn_.onClick = [&apvts]
    {
        if (auto* p = apvts.getParameter("lfoWave"))
            p->setValueNotifyingHost(1.0f);
    };

    // ── Transport buttons ─────────────────────────────────────
    addAndMakeVisible(playBtn_);
    addAndMakeVisible(stopBtn_);
    addAndMakeVisible(randomBtn_);
    addAndMakeVisible(clearBtn_);

    playBtn_.onClick = [this]
    {
        processor_.getEngine().sequencer.start();
    };
    stopBtn_.onClick = [this]
    {
        processor_.getEngine().sequencer.stop();
    };
    randomBtn_.onClick = [this]
    {
        processor_.getEngine().sequencer.randomize();
        refreshStepGrid();
    };
    clearBtn_.onClick = [this]
    {
        processor_.getEngine().sequencer.clear();
        refreshStepGrid();
    };

    // ── Edit mode buttons ─────────────────────────────────────
    for (auto* btn : { &gateModeBtn_, &accentModeBtn_, &slideModeBtn_ })
    {
        btn->setClickingTogglesState(true);
        btn->setRadioGroupId(3);
        addAndMakeVisible(btn);
    }
    gateModeBtn_.setToggleState(true, juce::dontSendNotification);

    gateModeBtn_.onClick    = [this] { setEditMode(EditMode::Gate); };
    accentModeBtn_.onClick  = [this] { setEditMode(EditMode::Accent); };
    slideModeBtn_.onClick   = [this] { setEditMode(EditMode::Slide); };

    // ── Step grid ─────────────────────────────────────────────
    for (int i = 0; i < acid303::kSeqSteps; ++i)
    {
        addAndMakeVisible(stepButtons_[static_cast<size_t>(i)]);
        stepButtons_[static_cast<size_t>(i)].onClick = [this, i]
        {
            auto& s = processor_.getEngine().sequencer.step(i);
            switch (editMode_)
            {
                case EditMode::Gate:   s.gate   = !s.gate;   break;
                case EditMode::Accent: s.accent = !s.accent; break;
                case EditMode::Slide:  s.slide  = !s.slide;  break;
            }
            refreshStepGrid();
        };

        noteLabels_[static_cast<size_t>(i)].setJustificationType(juce::Justification::centred);
        noteLabels_[static_cast<size_t>(i)].setFont(juce::FontOptions(10.0f));
        noteLabels_[static_cast<size_t>(i)].setColour(juce::Label::textColourId, kDimTextColour);
        addAndMakeVisible(noteLabels_[static_cast<size_t>(i)]);
    }

    refreshStepGrid();

    // Timer for step highlight (30fps)
    startTimerHz(30);
}

Acid303Editor::~Acid303Editor()
{
    stopTimer();
}

// ── Paint ─────────────────────────────────────────────────────
void Acid303Editor::paint(juce::Graphics& g)
{
    g.fillAll(kBgColour);

    // Draw section backgrounds
    auto drawSection = [&](juce::Rectangle<int> bounds)
    {
        g.setColour(kSectionBg);
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    };

    // Sections are drawn behind controls — using approximate bounds
    const int sectionY1 = 50;
    const int sectionH1 = 160;
    const int sectionY2 = sectionY1 + sectionH1 + 10;
    const int sectionH2 = 160;
    const int pad = 10;

    // Row 1: Osc | Filter | Envelope | LFO
    drawSection({ pad, sectionY1, 160, sectionH1 });
    drawSection({ pad + 170, sectionY1, 250, sectionH1 });
    drawSection({ pad + 430, sectionY1, 200, sectionH1 });
    drawSection({ pad + 640, sectionY1, 250, sectionH1 });

    // Row 2: Dist | Delay | Reverb
    drawSection({ pad, sectionY2, 180, sectionH2 });
    drawSection({ pad + 190, sectionY2, 310, sectionH2 });
    drawSection({ pad + 510, sectionY2, 220, sectionH2 });

    // Sequencer background
    const int seqY = sectionY2 + sectionH2 + 10;
    drawSection({ pad, seqY, kWidth - 2 * pad, kHeight - seqY - pad });
}

// ── Resized ───────────────────────────────────────────────────
void Acid303Editor::resized()
{
    const int pad = 10;
    const int knobW = 70;
    const int knobH = 80;
    const int labelH = 18;
    const int sectionY1 = 50;
    const int sectionH1 = 160;
    const int sectionY2 = sectionY1 + sectionH1 + 10;

    // ── Title row ─────────────────────────────────────────────
    titleLabel_.setBounds(pad, 8, 150, 30);
    subtitleLabel_.setBounds(160, 14, 150, 20);

    // ── Row 1: Oscillator ─────────────────────────────────────
    int x = pad + 5;
    int y = sectionY1 + 5;
    oscLabel_.setBounds(x, y, 150, labelH);
    y += labelH + 5;
    sawBtn_.setBounds(x, y, 45, 25);
    sqrBtn_.setBounds(x + 50, y, 45, 25);
    y += 30;
    tuningKnob_.setBounds(x, y, knobW, knobH);

    // ── Row 1: Filter ─────────────────────────────────────────
    x = pad + 175;
    y = sectionY1 + 5;
    filterLabel_.setBounds(x, y, 240, labelH);
    y += labelH + 5;
    cutoffKnob_.setBounds(x, y, knobW, knobH);
    resoKnob_.setBounds(x + knobW + 5, y, knobW, knobH);
    envModKnob_.setBounds(x + 2 * (knobW + 5), y, knobW, knobH);

    // ── Row 1: Envelope ───────────────────────────────────────
    x = pad + 435;
    y = sectionY1 + 5;
    envLabel_.setBounds(x, y, 190, labelH);
    y += labelH + 5;
    decayKnob_.setBounds(x, y, knobW, knobH);
    accentKnob_.setBounds(x + knobW + 5, y, knobW, knobH);

    // ── Row 1: LFO ────────────────────────────────────────────
    x = pad + 645;
    y = sectionY1 + 5;
    lfoLabel_.setBounds(x, y, 240, labelH);
    y += labelH + 5;
    lfoSineBtn_.setBounds(x, y, 35, 25);
    lfoTriBtn_.setBounds(x + 38, y, 35, 25);
    lfoSqrBtn_.setBounds(x + 76, y, 35, 25);
    y += 30;
    lfoRateKnob_.setBounds(x, y, knobW, knobH);
    lfoAmountKnob_.setBounds(x + knobW + 5, y, knobW, knobH);

    // ── Row 2: Distortion ─────────────────────────────────────
    x = pad + 5;
    y = sectionY2 + 5;
    distLabel_.setBounds(x, y, 170, labelH);
    y += labelH + 5;
    distToggle_.setBounds(x, y, 50, 25);
    y += 28;
    distAmountKnob_.setBounds(x, y, knobW, knobH);

    // ── Row 2: Delay ──────────────────────────────────────────
    x = pad + 195;
    y = sectionY2 + 5;
    delayLabel_.setBounds(x, y, 300, labelH);
    y += labelH + 5;
    delayToggle_.setBounds(x, y, 50, 25);
    y += 28;
    delayTimeKnob_.setBounds(x, y, knobW, knobH);
    delayFbKnob_.setBounds(x + knobW + 5, y, knobW, knobH);
    delayMixKnob_.setBounds(x + 2 * (knobW + 5), y, knobW, knobH);

    // ── Row 2: Reverb ─────────────────────────────────────────
    x = pad + 515;
    y = sectionY2 + 5;
    reverbLabel_.setBounds(x, y, 210, labelH);
    y += labelH + 5;
    reverbToggle_.setBounds(x, y, 50, 25);
    y += 28;
    reverbDecayKnob_.setBounds(x, y, knobW, knobH);
    reverbMixKnob_.setBounds(x + knobW + 5, y, knobW, knobH);

    // ── Sequencer ─────────────────────────────────────────────
    const int seqY = sectionY2 + 165;
    x = pad + 5;
    y = seqY + 5;

    seqLabel_.setBounds(x, y, 200, labelH);

    // Transport + tempo
    const int transportX = 220;
    playBtn_.setBounds(transportX, y, 50, 25);
    stopBtn_.setBounds(transportX + 55, y, 50, 25);
    randomBtn_.setBounds(transportX + 110, y, 65, 25);
    clearBtn_.setBounds(transportX + 180, y, 55, 25);
    tempoKnob_.setBounds(transportX + 250, y - 5, knobW, knobH);

    // Edit mode buttons
    y += labelH + 12;
    gateModeBtn_.setBounds(x, y, 55, 25);
    accentModeBtn_.setBounds(x + 60, y, 65, 25);
    slideModeBtn_.setBounds(x + 130, y, 55, 25);

    // Step grid
    y += 30;
    const int stepW = (kWidth - 2 * pad - 30) / acid303::kSeqSteps;
    for (int i = 0; i < acid303::kSeqSteps; ++i)
    {
        const int sx = pad + 5 + i * stepW;
        stepButtons_[static_cast<size_t>(i)].setBounds(sx, y, stepW - 4, 35);
        noteLabels_[static_cast<size_t>(i)].setBounds(sx, y + 37, stepW - 4, 16);
    }
}

// ── Timer callback — update step highlight ────────────────────
void Acid303Editor::timerCallback()
{
    const int currentStep = processor_.getEngine().sequencer.getCurrentStep();

    for (int i = 0; i < acid303::kSeqSteps; ++i)
        stepButtons_[static_cast<size_t>(i)].setHighlighted(i == currentStep);
}

// ── Refresh step grid toggles ─────────────────────────────────
void Acid303Editor::refreshStepGrid()
{
    for (int i = 0; i < acid303::kSeqSteps; ++i)
    {
        const auto& s = processor_.getEngine().sequencer.step(i);
        bool state = false;
        switch (editMode_)
        {
            case EditMode::Gate:   state = s.gate;   break;
            case EditMode::Accent: state = s.accent; break;
            case EditMode::Slide:  state = s.slide;  break;
        }
        stepButtons_[static_cast<size_t>(i)].setToggleState(
            state, juce::dontSendNotification);

        noteLabels_[static_cast<size_t>(i)].setText(
            juce::String(acid303::noteToName(s.note)),
            juce::dontSendNotification);
    }
}

// ── Set edit mode ─────────────────────────────────────────────
void Acid303Editor::setEditMode(EditMode mode)
{
    editMode_ = mode;
    refreshStepGrid();
}

// ── Setup section label style ─────────────────────────────────
void Acid303Editor::setupSectionLabel(juce::Label& label)
{
    label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, kLabelColour);
    addAndMakeVisible(label);
}
