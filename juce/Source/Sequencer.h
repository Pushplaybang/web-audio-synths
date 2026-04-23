#pragma once
// ACID-303 JUCE — 16-Step Sequencer
//
// Sample-accurate step sequencer running on the audio thread.
// Direct port of js/sequencer.js — uses a sample counter for timing
// rather than relying on host transport (works in Standalone mode too).

#include "SynthSpec.h"
#include <array>
#include <random>
#include <functional>

namespace acid303
{

class Sequencer
{
public:
    using StepCallback = std::function<void(int step)>;
    using NoteCallback = std::function<void(int midi, bool accent, bool slide)>;
    using ReleaseCallback = std::function<void()>;

    Sequencer() { initPattern(); }

    void prepare(float sampleRate) { sampleRate_ = sampleRate; }

    // ── Step data access ──────────────────────────────────────
    StepData& step(int index) { return steps_[static_cast<size_t>(index)]; }
    const StepData& step(int index) const { return steps_[static_cast<size_t>(index)]; }

    // ── Transport ─────────────────────────────────────────────
    int  getTempo()      const { return tempo_; }
    bool isPlaying()     const { return playing_; }
    int  getCurrentStep() const { return currentStep_; }

    void setTempo(int bpm)
    {
        tempo_ = std::clamp(bpm, kTempoMin, kTempoMax);
    }

    void start()
    {
        if (playing_) return;
        playing_ = true;
        currentStep_ = -1;
        sampleCounter_ = 0;
    }

    void stop()
    {
        playing_ = false;
        currentStep_ = -1;
        if (onRelease) onRelease();
        if (onStep) onStep(-1);
    }

    // ── Callbacks ─────────────────────────────────────────────
    StepCallback    onStep;
    NoteCallback    onNote;
    ReleaseCallback onRelease;

    // ── Audio-thread tick ─────────────────────────────────────
    /// Call once per sample from the audio thread.
    void processSample()
    {
        if (!playing_) return;

        const float stepDuration = 60.0f / static_cast<float>(tempo_) / 4.0f;
        const int samplesPerStep = static_cast<int>(stepDuration * sampleRate_);

        // Check if we need to release (at 75% of step duration)
        if (releaseCountdown_ > 0)
        {
            --releaseCountdown_;
            if (releaseCountdown_ == 0 && onRelease)
                onRelease();
        }

        ++sampleCounter_;
        if (sampleCounter_ >= samplesPerStep)
        {
            sampleCounter_ -= samplesPerStep;
            currentStep_ = (currentStep_ + 1) % kSeqSteps;
            handleStep(currentStep_, stepDuration);
        }
    }

    // ── Pattern management ────────────────────────────────────
    void initPattern()
    {
        for (int i = 0; i < kSeqSteps; ++i)
            steps_[static_cast<size_t>(i)] = kDefaultPattern[static_cast<size_t>(i)];
    }

    void clear()
    {
        for (auto& s : steps_)
            s = StepData{};
    }

    void randomize()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> scaleDist(0, static_cast<int>(kRandomScale.size()) - 1);

        for (auto& s : steps_)
        {
            const int octaveBase = (dist(gen) < 0.3f) ? 48 : 36;
            s.note   = octaveBase + kRandomScale[static_cast<size_t>(scaleDist(gen))];
            s.gate   = dist(gen) < 0.7f;
            s.accent = dist(gen) < 0.25f;
            s.slide  = dist(gen) < 0.2f;
        }
    }

private:
    std::array<StepData, kSeqSteps> steps_;
    int   tempo_          = kTempoDefault;
    bool  playing_        = false;
    int   currentStep_    = -1;
    int   sampleCounter_  = 0;
    int   releaseCountdown_ = 0;
    float sampleRate_     = 44100.0f;

    void handleStep(int stepIdx, float stepDuration)
    {
        const auto& s = steps_[static_cast<size_t>(stepIdx)];
        const auto& next = steps_[static_cast<size_t>((stepIdx + 1) % kSeqSteps)];

        if (s.gate)
        {
            if (onNote) onNote(s.note, s.accent, s.slide);

            // Schedule release unless next step continues with slide
            if (!(next.slide && next.gate))
            {
                releaseCountdown_ = static_cast<int>(
                    stepDuration * kGateReleaseFraction * sampleRate_);
            }
            else
            {
                releaseCountdown_ = 0;  // Cancel any pending release
            }
        }

        if (onStep) onStep(stepIdx);
    }
};

} // namespace acid303
