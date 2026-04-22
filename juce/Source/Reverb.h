#pragma once
// ACID-303 JUCE — Reverb Effect
//
// 4-line FDN (Feedback Delay Network) algorithmic reverb.
// Uses prime-length delay lines for diffusion, matching the Godot implementation.

#include "SynthSpec.h"
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace acid303
{

class AcidReverb
{
public:
    void prepare(float sampleRate)
    {
        sampleRate_ = sampleRate;

        for (int i = 0; i < kNumLines; ++i)
        {
            buffers_[i].assign(static_cast<size_t>(kDelayLengths[i]), 0.0f);
            writePos_[i] = 0;
        }
    }

    void reset()
    {
        for (int i = 0; i < kNumLines; ++i)
        {
            std::fill(buffers_[i].begin(), buffers_[i].end(), 0.0f);
            writePos_[i] = 0;
        }
    }

    void setEnabled(bool e) { enabled_ = e; }
    void setDecay(float d) { decay_ = d; }
    void setMix(float m) { mix_ = m; }

    /// Process one sample through the reverb.
    float processSample(float input)
    {
        if (!enabled_) return input;

        // Feedback coefficient derived from decay time.
        // For a delay line of avgDelay samples at sampleRate Hz, we want the
        // signal to decay by 60 dB (factor 0.001) over 'decay' seconds.
        const double loopTime = kAvgDelay / static_cast<double>(sampleRate_);
        const float fb = static_cast<float>(
            std::pow(0.001, loopTime / static_cast<double>(std::max(decay_, 0.1f))));

        float wet = 0.0f;
        for (int i = 0; i < kNumLines; ++i)
        {
            auto& buf = buffers_[i];
            const int size = static_cast<int>(buf.size());
            const int readPos = (writePos_[i] + 1) % size;
            const float delayed = buf[static_cast<size_t>(readPos)];
            wet += delayed;

            // Write input + feedback
            buf[static_cast<size_t>(writePos_[i])] = input * 0.25f + delayed * fb;
            writePos_[i] = (writePos_[i] + 1) % size;
        }

        wet *= 0.25f;  // Average the 4 lines

        // Dry/wet mix (matching web: reverbDry = 1 - mix * 0.3)
        const float dryLevel = 1.0f - mix_ * 0.3f;
        return input * dryLevel + wet * mix_;
    }

private:
    static constexpr int kNumLines = 4;
    static constexpr std::array<int, 4> kDelayLengths = { 1557, 1617, 1491, 1422 };
    // Average of kDelayLengths = (1557+1617+1491+1422)/4 = 1521.75
    // Used for computing the feedback coefficient from the target decay time.
    static constexpr double kAvgDelay = 1521.75;

    bool  enabled_    = false;
    float decay_      = kReverbDecayDefault;
    float mix_        = kReverbMixDefault;
    float sampleRate_ = 44100.0f;

    std::array<std::vector<float>, kNumLines> buffers_;
    std::array<int, kNumLines> writePos_ {};
};

} // namespace acid303
