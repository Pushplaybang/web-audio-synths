#pragma once
// ACID-303 JUCE — LFO (Low Frequency Oscillator)
//
// Generates a low-frequency control signal used to modulate filter detune.
// Output is in the range [-1, +1] and gets scaled by lfoAmount (in cents).

#include "SynthSpec.h"
#include <cmath>

namespace acid303
{

class LFO
{
public:
    void reset() { phase_ = 0.0; }

    void setWave(LfoWave w) { wave_ = w; }
    LfoWave getWave() const { return wave_; }

    /// Generate one LFO sample. Returns value in [-1, +1].
    float processSample(float rate, float sampleRate)
    {
        float out = 0.0f;

        switch (wave_)
        {
            case LfoWave::Sine:
                out = std::sin(static_cast<float>(2.0 * M_PI * phase_));
                break;

            case LfoWave::Triangle:
                if (phase_ < 0.25)
                    out = static_cast<float>(phase_ * 4.0);
                else if (phase_ < 0.75)
                    out = static_cast<float>(2.0 - phase_ * 4.0);
                else
                    out = static_cast<float>(phase_ * 4.0 - 4.0);
                break;

            case LfoWave::Square:
                out = (phase_ < 0.5) ? 1.0f : -1.0f;
                break;
        }

        phase_ += static_cast<double>(rate) / static_cast<double>(sampleRate);
        phase_ -= std::floor(phase_);

        return out;
    }

private:
    double  phase_ = 0.0;
    LfoWave wave_  = LfoWave::Sine;
};

} // namespace acid303
