#pragma once
// ACID-303 JUCE — Main Synth Engine
//
// Owns the complete signal chain: Oscillator → Filter → VCA → Effects → Output.
// Processes audio sample-by-sample with the full DSP chain.
// Direct port of js/synth.js (AcidSynth class) and godot/scripts/audio/acid_synth_engine.gd.

#include "SynthSpec.h"
#include "DiodeLadderFilter.h"
#include "Oscillator.h"
#include "Envelope.h"
#include "LFO.h"
#include "Distortion.h"
#include "TapeDelay.h"
#include "Reverb.h"
#include "Sequencer.h"
#include <cmath>
#include <algorithm>

namespace acid303
{

class AcidSynthEngine
{
public:
    AcidSynthEngine()
    {
        sequencer.onNote = [this](int midi, bool accent, bool slide)
        {
            triggerNote(midi, accent, slide);
        };
        sequencer.onRelease = [this]()
        {
            releaseNote();
        };
    }

    void prepare(float sampleRate)
    {
        sampleRate_ = sampleRate;
        osc_.reset();
        filter_.reset();
        ampEnv_.reset();
        filterEnv_.reset();
        lfo_.reset();
        distortion_.prepare(sampleRate);
        delay_.prepare(sampleRate);
        reverb_.prepare(sampleRate);
        sequencer.prepare(sampleRate);
    }

    /// Generate a single audio sample through the full signal chain.
    float processSample()
    {
        // Tick the sequencer (sample-accurate)
        sequencer.processSample();

        // ── Slide handling ────────────────────────────────────
        if (isSliding_ && slideSamplesRemaining_ > 0)
        {
            currentFreq_ += slideFreqStep_;
            --slideSamplesRemaining_;
            if (slideSamplesRemaining_ <= 0)
            {
                currentFreq_ = targetFreq_;
                isSliding_ = false;
            }
        }

        // ── Oscillator ────────────────────────────────────────
        const float oscOut = osc_.processSample(
            static_cast<float>(currentFreq_), sampleRate_);

        // ── LFO → filter detune ───────────────────────────────
        const float lfoVal = lfo_.processSample(lfoRate, sampleRate_);
        const float detuneCents = lfoVal * lfoAmount;
        float filterFreq = cutoff * std::pow(2.0f, detuneCents / 1200.0f);

        // ── Filter envelope ───────────────────────────────────
        const float filtEnvVal = filterEnv_.processSample(decay, sampleRate_);
        filterFreq += filtEnvVal;

        // ── Diode ladder filter ───────────────────────────────
        const float filtered = static_cast<float>(
            filter_.processSample(
                static_cast<double>(filterFreq),
                static_cast<double>(resonance),
                static_cast<double>(sampleRate_),
                static_cast<double>(oscOut)));

        // ── VCA (amplitude envelope) ──────────────────────────
        const float amp = ampEnv_.processSample(decay, sampleRate_);
        float sample = filtered * amp;

        // ── Effects chain ─────────────────────────────────────
        sample = distortion_.processSample(sample);
        sample = delay_.processSample(sample);
        sample = reverb_.processSample(sample);

        // ── Master volume ─────────────────────────────────────
        sample *= kMasterVolume;

        // ── Soft clip (limiter equivalent) ────────────────────
        sample = std::clamp(sample, -1.0f, 1.0f);

        return sample;
    }

    // ── Note control API ──────────────────────────────────────
    void triggerNote(int midi, bool isAccent = false, bool isSlide = false)
    {
        const double freq = midiToFreq(midi + tuning);

        if (isSlide && currentNote_ >= 0)
        {
            targetFreq_ = freq;
            const int slideSamples = static_cast<int>(kSlideTime * sampleRate_);
            slideFreqStep_ = (targetFreq_ - currentFreq_) / std::max(static_cast<double>(slideSamples), 1.0);
            slideSamplesRemaining_ = slideSamples;
            isSliding_ = true;
        }
        else
        {
            currentFreq_ = freq;
            targetFreq_ = freq;
            isSliding_ = false;
            slideSamplesRemaining_ = 0;
        }

        if (!isSlide)
        {
            // Amplitude envelope
            const float accentAmt = isAccent ? accent : 0.0f;
            const float peakVol = kBaseVol + accentAmt * 0.4f;
            const float adjDecay = decay * (isAccent ? 0.7f : 1.0f);
            ampEnv_.trigger(peakVol, adjDecay, sampleRate_);

            // Filter envelope
            const float envDepth = envMod * (1.0f + accentAmt * 1.5f);
            const float peakCutoff = std::min(cutoff + envDepth, 12000.0f);
            filterEnv_.trigger(peakCutoff, adjDecay, sampleRate_);
        }

        currentNote_ = midi;
    }

    void releaseNote()
    {
        ampEnv_.release(sampleRate_);
        currentNote_ = -1;
    }

    // ── Parameters (public for direct access by the processor) ─
    float cutoff    = kCutoffDefault;
    float resonance = kResonanceDefault;
    float envMod    = kEnvModDefault;
    float decay     = kDecayDefault;
    float accent    = kAccentDefault;
    int   tuning    = kTuningDefault;
    float lfoRate   = kLfoRateDefault;
    float lfoAmount = kLfoAmountDefault;

    // ── Component access for parameter wiring ─────────────────
    Oscillator&     getOsc()        { return osc_; }
    Distortion&     getDistortion() { return distortion_; }
    TapeDelay&      getDelay()      { return delay_; }
    AcidReverb&     getReverb()     { return reverb_; }
    LFO&            getLfo()        { return lfo_; }

    Sequencer sequencer;

private:
    float sampleRate_ = 44100.0f;

    // DSP components
    Oscillator        osc_;
    DiodeLadderFilter filter_;
    Envelope          ampEnv_;
    Envelope          filterEnv_;
    LFO               lfo_;
    Distortion        distortion_;
    TapeDelay         delay_;
    AcidReverb        reverb_;

    // Voice state
    int    currentNote_ = -1;
    double currentFreq_ = 0.0;
    double targetFreq_  = 0.0;
    bool   isSliding_   = false;
    int    slideSamplesRemaining_ = 0;
    double slideFreqStep_ = 0.0;
};

} // namespace acid303
