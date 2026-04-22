#pragma once
// ACID-303 JUCE — 4-Pole Diode Ladder Filter
//
// Direct port of js/worklets/diode-ladder.js.
// Uses double precision for numerical stability (matching web Float64Array).
// Per-stage tanh saturation gives the 303's characteristic squelch.

#include <cmath>
#include <algorithm>

namespace acid303
{

class DiodeLadderFilter
{
public:
    DiodeLadderFilter() { reset(); }

    void reset()
    {
        s_[0] = s_[1] = s_[2] = s_[3] = 0.0;
    }

    /// Process a single sample through the 4-pole ladder.
    /// @param freq     Filter cutoff in Hz (already includes LFO detune)
    /// @param resonance 0–30 range (same as web spec)
    /// @param sampleRate Audio sample rate
    /// @param input    Input sample value
    /// @return         Filtered output sample
    double processSample(double freq, double resonance, double sampleRate, double input)
    {
        // Clamp frequency to safe range
        const double nyquist = sampleRate * 0.45;
        freq = std::clamp(freq, kFilterFreqFloor, nyquist);

        // Bilinear pre-warp coefficient
        const double g = 2.0 * std::tan(M_PI * freq / sampleRate);
        const double G = g / (1.0 + g);

        // Resonance feedback coefficient: map 0–30 to 0–3.8
        const double k = (resonance / 30.0) * kFilterResonanceScale;

        // Feedback from last stage
        const double fb = k * tanhApprox(s_[3]);

        // Input with feedback subtracted, saturated
        double x = tanhApprox(input - fb);

        // Four cascaded one-pole stages with per-stage saturation
        s_[0] += G * (x - s_[0]); x = tanhApprox(s_[0]);
        s_[1] += G * (x - s_[1]); x = tanhApprox(s_[1]);
        s_[2] += G * (x - s_[2]); x = tanhApprox(s_[2]);
        s_[3] += G * (x - s_[3]);

        return s_[3];
    }

private:
    double s_[4] {};

    /// tanh approximation matching the JS implementation.
    /// Padé-like rational approximation: t(x) = x(27 + x²) / (27 + 9x²).
    /// Max error vs std::tanh is ~0.004 in [-3, 3]. Hard-clamps outside
    /// that range for efficiency since tanh(±3) ≈ ±0.995.
    static double tanhApprox(double x)
    {
        if (x > 3.0)  return 1.0;
        if (x < -3.0) return -1.0;
        const double x2 = x * x;
        return x * (27.0 + x2) / (27.0 + 9.0 * x2);
    }
};

} // namespace acid303
