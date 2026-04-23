# ACID-303 Godot — Synth Behavior Constants
#
# Auto-derived from spec/synth-behavior.json. This is the single source of
# truth for parameter ranges, defaults, and envelope shapes across platforms.
class_name SynthSpec

# ── Oscillator ─────────────────────────────────────────────────
enum Waveform { SAW, SQUARE }

const WAVEFORM_DEFAULT := Waveform.SAW
const TUNING_MIN := -12
const TUNING_MAX := 12
const TUNING_DEFAULT := 0

# ── Filter ─────────────────────────────────────────────────────
const CUTOFF_MIN := 60.0
const CUTOFF_MAX := 8000.0
const CUTOFF_DEFAULT := 800.0
const RESONANCE_MIN := 0.0
const RESONANCE_MAX := 30.0
const RESONANCE_DEFAULT := 8.0
const ENVMOD_MIN := 1.0
const ENVMOD_MAX := 8000.0
const ENVMOD_DEFAULT := 3000.0

# ── Envelope ───────────────────────────────────────────────────
const DECAY_MIN := 0.02
const DECAY_MAX := 1.5
const DECAY_DEFAULT := 0.3
const ACCENT_MIN := 0.0
const ACCENT_MAX := 1.0
const ACCENT_DEFAULT := 0.6

# ── Distortion ─────────────────────────────────────────────────
const DIST_AMOUNT_MIN := 0.0
const DIST_AMOUNT_MAX := 100.0
const DIST_AMOUNT_DEFAULT := 40.0

# ── Delay ──────────────────────────────────────────────────────
const DELAY_TIME_MIN := 0.05
const DELAY_TIME_MAX := 1.0
const DELAY_TIME_DEFAULT := 0.375
const DELAY_FEEDBACK_MIN := 0.0
const DELAY_FEEDBACK_MAX := 0.9
const DELAY_FEEDBACK_DEFAULT := 0.45
const DELAY_MIX_MIN := 0.0
const DELAY_MIX_MAX := 1.0
const DELAY_MIX_DEFAULT := 0.3

# ── Reverb ─────────────────────────────────────────────────────
const REVERB_DECAY_MIN := 0.3
const REVERB_DECAY_MAX := 5.0
const REVERB_DECAY_DEFAULT := 2.0
const REVERB_MIX_MIN := 0.0
const REVERB_MIX_MAX := 1.0
const REVERB_MIX_DEFAULT := 0.2

# ── LFO ────────────────────────────────────────────────────────
enum LfoWave { SINE, TRIANGLE, SQUARE }

const LFO_RATE_MIN := 0.05
const LFO_RATE_MAX := 30.0
const LFO_RATE_DEFAULT := 4.0
const LFO_AMOUNT_MIN := 0.0
const LFO_AMOUNT_MAX := 4800.0
const LFO_AMOUNT_DEFAULT := 0.0
const LFO_WAVE_DEFAULT := LfoWave.SINE

# ── Sequencer ──────────────────────────────────────────────────
const SEQ_STEPS := 16
const NOTE_MIN := 24
const NOTE_MAX := 72
const NOTE_DEFAULT := 36
const TEMPO_MIN := 40
const TEMPO_MAX := 300
const TEMPO_DEFAULT := 138

# ── Audio constants ────────────────────────────────────────────
const FILTER_FREQ_FLOOR := 30.0
const FILTER_RESONANCE_SCALE := 3.8  # Internal max feedback coefficient
const MASTER_VOLUME := 0.7
const SLIDE_TIME := 0.06  # seconds
const AMP_ATTACK := 0.005
const FILTER_ATTACK := 0.003
const BASE_VOL := 0.3
const GATE_RELEASE_FRACTION := 0.75  # noteOff at 75% of step duration

# ── MIDI / keyboard ───────────────────────────────────────────
const KEYBOARD_BASE_OCTAVE := 48
const NOTE_NAMES := ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]
const RANDOM_SCALE := [0, 3, 5, 7, 10, 12]

# ── Default pattern ────────────────────────────────────────────
# Each entry: [note, gate, accent, slide]
const DEFAULT_PATTERN := [
	[36, true, false, false], [36, true, true, false],
	[39, true, false, false], [36, true, false, true],
	[48, true, true, false],  [36, false, false, false],
	[39, true, false, false], [41, true, true, false],
	[36, true, false, false], [36, true, false, true],
	[48, true, false, false], [46, true, true, true],
	[36, true, false, false], [39, false, false, false],
	[41, true, true, false],  [43, true, false, true],
]

# ── Helpers ────────────────────────────────────────────────────

static func midi_to_freq(midi_note: int) -> float:
	return 440.0 * pow(2.0, (midi_note - 69) / 12.0)


static func note_to_name(midi_note: int) -> String:
	return NOTE_NAMES[midi_note % 12] + str(int(midi_note / 12) - 1)
