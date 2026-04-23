#pragma once
// ACID-303 JUCE — Synth Behavior Constants
//
// Derived from spec/synth-behavior.json. Single source of truth for
// parameter ranges, defaults, and audio constants across all platforms.

#include <cmath>
#include <array>
#include <string>

namespace acid303
{

// ── Oscillator ────────────────────────────────────────────────
enum class Waveform { Sawtooth, Square };

inline constexpr int    kTuningMin     = -12;
inline constexpr int    kTuningMax     = 12;
inline constexpr int    kTuningDefault = 0;

// ── Filter ────────────────────────────────────────────────────
inline constexpr float kCutoffMin      = 60.0f;
inline constexpr float kCutoffMax      = 8000.0f;
inline constexpr float kCutoffDefault  = 800.0f;
inline constexpr float kResonanceMin   = 0.0f;
inline constexpr float kResonanceMax   = 30.0f;
inline constexpr float kResonanceDefault = 8.0f;
inline constexpr float kEnvModMin      = 1.0f;
inline constexpr float kEnvModMax      = 8000.0f;
inline constexpr float kEnvModDefault  = 3000.0f;

// ── Envelope ──────────────────────────────────────────────────
inline constexpr float kDecayMin       = 0.02f;
inline constexpr float kDecayMax       = 1.5f;
inline constexpr float kDecayDefault   = 0.3f;
inline constexpr float kAccentMin      = 0.0f;
inline constexpr float kAccentMax      = 1.0f;
inline constexpr float kAccentDefault  = 0.6f;

// ── Distortion ────────────────────────────────────────────────
inline constexpr float kDistAmountMin      = 0.0f;
inline constexpr float kDistAmountMax      = 100.0f;
inline constexpr float kDistAmountDefault  = 40.0f;

// ── Delay ─────────────────────────────────────────────────────
inline constexpr float kDelayTimeMin       = 0.05f;
inline constexpr float kDelayTimeMax       = 1.0f;
inline constexpr float kDelayTimeDefault   = 0.375f;
inline constexpr float kDelayFeedbackMin   = 0.0f;
inline constexpr float kDelayFeedbackMax   = 0.9f;
inline constexpr float kDelayFeedbackDefault = 0.45f;
inline constexpr float kDelayMixMin        = 0.0f;
inline constexpr float kDelayMixMax        = 1.0f;
inline constexpr float kDelayMixDefault    = 0.3f;

// ── Reverb ────────────────────────────────────────────────────
inline constexpr float kReverbDecayMin     = 0.3f;
inline constexpr float kReverbDecayMax     = 5.0f;
inline constexpr float kReverbDecayDefault = 2.0f;
inline constexpr float kReverbMixMin       = 0.0f;
inline constexpr float kReverbMixMax       = 1.0f;
inline constexpr float kReverbMixDefault   = 0.2f;

// ── LFO ───────────────────────────────────────────────────────
enum class LfoWave { Sine, Triangle, Square };

inline constexpr float kLfoRateMin         = 0.05f;
inline constexpr float kLfoRateMax         = 30.0f;
inline constexpr float kLfoRateDefault     = 4.0f;
inline constexpr float kLfoAmountMin       = 0.0f;
inline constexpr float kLfoAmountMax       = 4800.0f;
inline constexpr float kLfoAmountDefault   = 0.0f;

// ── Sequencer ─────────────────────────────────────────────────
inline constexpr int   kSeqSteps      = 16;
inline constexpr int   kNoteMin       = 24;
inline constexpr int   kNoteMax       = 72;
inline constexpr int   kNoteDefault   = 36;
inline constexpr int   kTempoMin      = 40;
inline constexpr int   kTempoMax      = 300;
inline constexpr int   kTempoDefault  = 138;

// ── Audio constants ───────────────────────────────────────────
inline constexpr double kFilterFreqFloor       = 30.0;
inline constexpr double kFilterResonanceScale  = 3.8;
inline constexpr float  kMasterVolume          = 0.7f;
inline constexpr float  kSlideTime             = 0.06f;  // seconds
inline constexpr float  kAmpAttack             = 0.005f;
inline constexpr float  kFilterAttack           = 0.003f;
inline constexpr float  kBaseVol               = 0.3f;
inline constexpr float  kGateReleaseFraction   = 0.75f;

// ── Step data ─────────────────────────────────────────────────
struct StepData
{
    int  note   = kNoteDefault;
    bool gate   = false;
    bool accent = false;
    bool slide  = false;
};

// ── Default pattern ───────────────────────────────────────────
inline const std::array<StepData, kSeqSteps> kDefaultPattern = {{
    { 36, true,  false, false }, { 36, true,  true,  false },
    { 39, true,  false, false }, { 36, true,  false, true  },
    { 48, true,  true,  false }, { 36, false, false, false },
    { 39, true,  false, false }, { 41, true,  true,  false },
    { 36, true,  false, false }, { 36, true,  false, true  },
    { 48, true,  false, false }, { 46, true,  true,  true  },
    { 36, true,  false, false }, { 39, false, false, false },
    { 41, true,  true,  false }, { 43, true,  false, true  },
}};

// ── Minor pentatonic scale intervals for randomization ────────
inline constexpr std::array<int, 6> kRandomScale = { 0, 3, 5, 7, 10, 12 };

// ── Helpers ───────────────────────────────────────────────────
inline double midiToFreq(int midiNote)
{
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}

inline constexpr std::array<const char*, 12> kNoteNames = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

inline std::string noteToName(int midiNote)
{
    return std::string(kNoteNames[midiNote % 12]) + std::to_string(midiNote / 12 - 1);
}

} // namespace acid303
