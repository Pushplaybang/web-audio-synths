#pragma once
// ACID-303 JUCE — Plugin Editor (GUI)
//
// Custom GUI matching the web version's control layout:
// - Oscillator section (waveform, tuning)
// - Filter section (cutoff, resonance, env mod)
// - Envelope section (decay, accent)
// - LFO section (rate, amount, wave)
// - Effects sections (distortion, delay, reverb)
// - Step sequencer grid with gate/accent/slide modes
// - Transport controls (play, stop, random, tempo)

#include <JuceHeader.h>
#include "PluginProcessor.h"

// ── Custom rotary knob ────────────────────────────────────────
class AcidKnob : public juce::Slider
{
public:
    AcidKnob(const juce::String& name = "")
    {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        setName(name);
    }
};

// ── Step button for sequencer grid ────────────────────────────
class StepButton : public juce::TextButton
{
public:
    StepButton()
    {
        setClickingTogglesState(true);
    }

    void setHighlighted(bool h)
    {
        highlighted_ = h;
        repaint();
    }

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);

        // Background
        if (getToggleState())
            g.setColour(juce::Colour(0xffff5722));  // Orange-red for active
        else
            g.setColour(juce::Colour(0xff2a2a33));  // Dark grey

        if (highlighted_)
            g.setColour(getToggleState()
                ? juce::Colour(0xffff8a65)    // Bright orange when active+playing
                : juce::Colour(0xff4a4a55));  // Lighter grey for current step

        g.fillRoundedRectangle(bounds, 3.0f);

        // Border
        g.setColour(shouldDrawButtonAsHighlighted
            ? juce::Colour(0xffaaaaaa)
            : juce::Colour(0xff555555));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
    }

private:
    bool highlighted_ = false;
};

// ── Main editor ───────────────────────────────────────────────
class Acid303Editor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit Acid303Editor(Acid303Processor&);
    ~Acid303Editor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    Acid303Processor& processor_;

    // ── Section labels ────────────────────────────────────────
    juce::Label titleLabel_{ {}, "ACID-303" };
    juce::Label subtitleLabel_{ {}, "VST/AU Synthesizer" };

    // ── Oscillator ────────────────────────────────────────────
    juce::Label oscLabel_{ {}, "OSCILLATOR" };
    juce::TextButton sawBtn_{ "SAW" };
    juce::TextButton sqrBtn_{ "SQR" };
    AcidKnob tuningKnob_{ "Tuning" };

    // ── Filter ────────────────────────────────────────────────
    juce::Label filterLabel_{ {}, "FILTER" };
    AcidKnob cutoffKnob_{ "Cutoff" };
    AcidKnob resoKnob_{ "Reso" };
    AcidKnob envModKnob_{ "EnvMod" };

    // ── Envelope ──────────────────────────────────────────────
    juce::Label envLabel_{ {}, "ENVELOPE" };
    AcidKnob decayKnob_{ "Decay" };
    AcidKnob accentKnob_{ "Accent" };

    // ── LFO ───────────────────────────────────────────────────
    juce::Label lfoLabel_{ {}, "LFO → FILTER" };
    AcidKnob lfoRateKnob_{ "Rate" };
    AcidKnob lfoAmountKnob_{ "Amount" };
    juce::TextButton lfoSineBtn_{ "SIN" };
    juce::TextButton lfoTriBtn_{ "TRI" };
    juce::TextButton lfoSqrBtn_{ "SQR" };

    // ── Distortion ────────────────────────────────────────────
    juce::Label distLabel_{ {}, "DISTORTION" };
    juce::ToggleButton distToggle_{ "On" };
    AcidKnob distAmountKnob_{ "Drive" };

    // ── Delay ─────────────────────────────────────────────────
    juce::Label delayLabel_{ {}, "TAPE DELAY" };
    juce::ToggleButton delayToggle_{ "On" };
    AcidKnob delayTimeKnob_{ "Time" };
    AcidKnob delayFbKnob_{ "Fdbk" };
    AcidKnob delayMixKnob_{ "Mix" };

    // ── Reverb ────────────────────────────────────────────────
    juce::Label reverbLabel_{ {}, "REVERB" };
    juce::ToggleButton reverbToggle_{ "On" };
    AcidKnob reverbDecayKnob_{ "Decay" };
    AcidKnob reverbMixKnob_{ "Mix" };

    // ── Transport / Sequencer ─────────────────────────────────
    juce::Label seqLabel_{ {}, "STEP SEQUENCER" };
    juce::TextButton playBtn_{ "PLAY" };
    juce::TextButton stopBtn_{ "STOP" };
    juce::TextButton randomBtn_{ "RANDOM" };
    juce::TextButton clearBtn_{ "CLEAR" };
    AcidKnob tempoKnob_{ "Tempo" };

    // Sequencer mode buttons
    juce::TextButton gateModeBtn_{ "GATE" };
    juce::TextButton accentModeBtn_{ "ACCENT" };
    juce::TextButton slideModeBtn_{ "SLIDE" };
    enum class EditMode { Gate, Accent, Slide };
    EditMode editMode_ = EditMode::Gate;

    // Step grid (16 buttons)
    std::array<StepButton, acid303::kSeqSteps> stepButtons_;
    std::array<juce::Label, acid303::kSeqSteps> noteLabels_;

    // ── APVTS attachments ─────────────────────────────────────
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> cutoffAttach_, resoAttach_, envModAttach_;
    std::unique_ptr<SliderAttachment> decayAttach_, accentAttach_;
    std::unique_ptr<SliderAttachment> tuningAttach_;
    std::unique_ptr<SliderAttachment> distAmountAttach_;
    std::unique_ptr<ButtonAttachment> distOnAttach_;
    std::unique_ptr<SliderAttachment> delayTimeAttach_, delayFbAttach_, delayMixAttach_;
    std::unique_ptr<ButtonAttachment> delayOnAttach_;
    std::unique_ptr<SliderAttachment> reverbDecayAttach_, reverbMixAttach_;
    std::unique_ptr<ButtonAttachment> reverbOnAttach_;
    std::unique_ptr<SliderAttachment> lfoRateAttach_, lfoAmountAttach_;
    std::unique_ptr<SliderAttachment> tempoAttach_;

    // ── Helpers ───────────────────────────────────────────────
    void timerCallback() override;
    void refreshStepGrid();
    void setEditMode(EditMode mode);
    void setupSectionLabel(juce::Label& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Acid303Editor)
};
