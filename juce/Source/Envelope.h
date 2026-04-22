#pragma once
// ACID-303 JUCE — Envelope Generator
//
// Produces amplitude and filter envelopes matching the web implementation.
// Uses exponential decay segments and tracks envelope phase per-sample.

#include "SynthSpec.h"
#include <cmath>
#include <algorithm>

namespace acid303
{

class Envelope
{
public:
    enum class Phase { Off, Attack, Decay, Release };

    Envelope() { reset(); }

    void reset()
    {
        phase_ = Phase::Off;
        value_ = 0.0f;
        target_ = 0.0f;
        rate_ = 0.0f;
    }

    float getValue() const { return value_; }
    bool isOff() const { return phase_ == Phase::Off; }

    /// Trigger the envelope (note on).
    void trigger(float peak, float decayTime, float sampleRate)
    {
        phase_ = Phase::Attack;
        target_ = peak;
        // Linear attack over 5ms
        const float attackSamples = std::max(kAmpAttack * sampleRate, 1.0f);
        rate_ = (peak - 0.001f) / attackSamples;
        value_ = 0.001f;  // Start just above zero
    }

    /// Release the envelope (note off).
    void release(float sampleRate)
    {
        if (phase_ == Phase::Off) return;
        phase_ = Phase::Release;
        // Time constant of 10ms (matching web: setTargetAtTime tc=0.01)
        rate_ = std::exp(-1.0f / (0.01f * sampleRate));
        target_ = 0.0f;
    }

    /// Advance envelope by one sample.
    float processSample(float decayTime, float sampleRate)
    {
        switch (phase_)
        {
            case Phase::Off:
                value_ = 0.0f;
                break;

            case Phase::Attack:
                value_ += rate_;
                if (value_ >= target_)
                {
                    value_ = target_;
                    // Transition to decay: exponential decay.
                    // The 0.33 factor compresses the time constant so the decay
                    // audibly reaches near-zero within the specified decayTime,
                    // matching the web version's exponentialRampToValueAtTime feel.
                    phase_ = Phase::Decay;
                    const float sustainLevel = kBaseVol * 0.4f + 0.001f;
                    rate_ = std::exp(-1.0f / (decayTime * sampleRate * 0.33f));
                    target_ = sustainLevel;
                }
                break;

            case Phase::Decay:
                value_ = value_ * rate_ + target_ * (1.0f - rate_);
                if (value_ < 0.0001f)
                {
                    value_ = 0.0f;
                    phase_ = Phase::Off;
                }
                break;

            case Phase::Release:
                value_ *= rate_;
                if (value_ < 0.0001f)
                {
                    value_ = 0.0f;
                    phase_ = Phase::Off;
                }
                break;
        }
        return value_;
    }

private:
    Phase phase_ = Phase::Off;
    float value_  = 0.0f;
    float target_ = 0.0f;
    float rate_   = 0.0f;
};

} // namespace acid303
