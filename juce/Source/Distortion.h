#pragma once
// ACID-303 JUCE — Distortion Effect
//
// Oversampled waveshaper with 4x oversampling for high-quality audio.
// Transfer function: f(x) = ((1 + k) * x) / (1 + k * |x|) where k = amount * 2
// Matches the web implementation but adds 4x oversampling to suppress aliasing.

#include "SynthSpec.h"
#include <cmath>
#include <algorithm>
#include <array>

namespace acid303
{

class Distortion
{
public:
    void prepare(float sampleRate)
    {
        sampleRate_ = sampleRate;
        // Initialize oversampling filter states
        for (auto& s : upsampleState_) s = 0.0f;
        for (auto& s : downsampleState_) s = 0.0f;
    }

    void setEnabled(bool e) { enabled_ = e; }
    bool isEnabled() const { return enabled_; }

    void setAmount(float a) { amount_ = a; }
    float getAmount() const { return amount_; }

    /// Process a single sample through the waveshaper with 4x oversampling.
    /// Uses simple linear-interpolation upsample (4 evenly spaced points between
    /// consecutive input samples), then applies the waveshaper, then averages back
    /// down with a one-pole anti-alias filter. This suppresses aliasing harmonics
    /// from the waveshaper's nonlinearity.
    float processSample(float input)
    {
        if (!enabled_) return input;

        const float k = amount_ * 2.0f;
        if (k < 0.001f) return input;

        // 4x oversampling: upsample → shape → downsample
        // Simple linear interpolation upsample
        std::array<float, 4> oversampled;
        oversampled[0] = 0.75f * lastInput_ + 0.25f * input;
        oversampled[1] = 0.50f * lastInput_ + 0.50f * input;
        oversampled[2] = 0.25f * lastInput_ + 0.75f * input;
        oversampled[3] = input;
        lastInput_ = input;

        // Apply waveshaper to each oversampled point
        for (auto& s : oversampled)
            s = waveshape(s, k);

        // Simple averaging downsample with one-pole anti-alias filter
        float sum = 0.0f;
        for (auto s : oversampled)
        {
            downsampleState_[0] += 0.5f * (s - downsampleState_[0]);
            sum += downsampleState_[0];
        }

        return sum * 0.25f;
    }

private:
    bool  enabled_     = false;
    float amount_      = kDistAmountDefault;
    float sampleRate_  = 44100.0f;
    float lastInput_   = 0.0f;
    std::array<float, 2> upsampleState_   {};
    std::array<float, 2> downsampleState_ {};

    /// The waveshaper transfer function: ((1+k)*x) / (1+k*|x|)
    static float waveshape(float x, float k)
    {
        return ((1.0f + k) * x) / (1.0f + k * std::abs(x));
    }
};

} // namespace acid303
