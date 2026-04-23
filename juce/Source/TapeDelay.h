#pragma once
// ACID-303 JUCE — Tape Delay Effect
//
// Feedback delay with a one-pole lowpass filter in the feedback path at 3500 Hz,
// matching the web implementation.

#include "SynthSpec.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace acid303
{

class TapeDelay
{
public:
    void prepare(float sampleRate, float maxDelaySec = 2.0f)
    {
        sampleRate_ = sampleRate;
        bufferSize_ = static_cast<int>(sampleRate * maxDelaySec);
        buffer_.assign(static_cast<size_t>(bufferSize_), 0.0f);
        writePos_ = 0;
        filterState_ = 0.0f;

        // Precompute the one-pole lowpass coefficient for the feedback path.
        // Standard RC-filter approximation: coeff = 1 - e^(-2π·f/sr)
        lpCoeff_ = 1.0f - std::exp(-2.0f * static_cast<float>(M_PI)
                                     * kFeedbackFilterFreq / sampleRate);
    }

    void reset()
    {
        std::fill(buffer_.begin(), buffer_.end(), 0.0f);
        writePos_ = 0;
        filterState_ = 0.0f;
    }

    void setEnabled(bool e) { enabled_ = e; }
    void setDelayTime(float t) { delayTime_ = t; }
    void setFeedback(float f) { feedback_ = f; }
    void setMix(float m) { mix_ = m; }

    /// Process a single sample through the delay.
    float processSample(float input)
    {
        if (!enabled_ || bufferSize_ == 0) return input;

        // Read from delay buffer
        int delaySamples = std::clamp(static_cast<int>(delayTime_ * sampleRate_),
                                       1, bufferSize_ - 1);
        int readPos = (writePos_ - delaySamples + bufferSize_) % bufferSize_;
        float delayed = buffer_[static_cast<size_t>(readPos)];

        // One-pole lowpass on feedback path
        filterState_ += lpCoeff_ * (delayed - filterState_);

        // Write input + filtered feedback to buffer
        buffer_[static_cast<size_t>(writePos_)] = input + filterState_ * feedback_;
        writePos_ = (writePos_ + 1) % bufferSize_;

        // Wet/dry mix
        return input * (1.0f - mix_) + delayed * mix_;
    }

private:
    static constexpr float kFeedbackFilterFreq = 3500.0f;

    bool  enabled_     = false;
    float delayTime_   = kDelayTimeDefault;
    float feedback_    = kDelayFeedbackDefault;
    float mix_         = kDelayMixDefault;
    float sampleRate_  = 44100.0f;
    float lpCoeff_     = 0.0f;
    float filterState_ = 0.0f;

    std::vector<float> buffer_;
    int bufferSize_ = 0;
    int writePos_   = 0;
};

} // namespace acid303
