#pragma once
// ACID-303 JUCE — Band-Limited Oscillator
//
// PolyBLEP-antialiased sawtooth and square waveforms for DAW-quality audio.
// Phase-continuous across frequency changes for glitch-free slides.

#include "SynthSpec.h"
#include <cmath>

namespace acid303
{

class Oscillator
{
public:
    void reset() { phase_ = 0.0; }

    void setWaveform(Waveform w) { waveform_ = w; }
    Waveform getWaveform() const { return waveform_; }

    /// Generate one sample at the given frequency.
    float processSample(float freq, float sampleRate)
    {
        const float dt = freq / sampleRate;  // phase increment
        float out = 0.0f;

        switch (waveform_)
        {
            case Waveform::Sawtooth:
                out = 2.0f * static_cast<float>(phase_) - 1.0f;
                out -= polyBLEP(phase_, dt);
                break;

            case Waveform::Square:
            {
                out = (phase_ < 0.5) ? 1.0f : -1.0f;
                out += polyBLEP(phase_, dt);
                out -= polyBLEP(std::fmod(phase_ + 0.5, 1.0), dt);
                break;
            }
        }

        // Advance phase, wrapping at 1.0
        phase_ += dt;
        phase_ -= std::floor(phase_);

        return out;
    }

private:
    double   phase_    = 0.0;
    Waveform waveform_ = Waveform::Sawtooth;

    /// PolyBLEP correction — reduces aliasing at discontinuities.
    /// Applied at phase boundaries for band-limited waveform generation.
    static float polyBLEP(double phase, float dt)
    {
        double t = phase;
        if (t < static_cast<double>(dt))
        {
            t /= static_cast<double>(dt);
            return static_cast<float>(t + t - t * t - 1.0);
        }
        else if (t > 1.0 - static_cast<double>(dt))
        {
            t = (t - 1.0) / static_cast<double>(dt);
            return static_cast<float>(t * t + t + t + 1.0);
        }
        return 0.0f;
    }
};

} // namespace acid303
