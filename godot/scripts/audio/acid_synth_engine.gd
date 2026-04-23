# ACID-303 Godot — Main Synth Engine
#
# Owns the complete signal chain: Oscillator → Filter → VCA → Effects → Output.
# Fills an AudioStreamGeneratorPlayback buffer each frame with sample-accurate DSP.
# Direct port of js/synth.js (AcidSynth class).
extends Node

class_name AcidSynthEngine

signal worklet_status(available: bool)

# ── DSP components ─────────────────────────────────────────────
var osc := Oscillator.new()
var sub_osc := Oscillator.new()
var filter := DiodeLadderFilter.new()
var amp_env := Envelope.new()
var filter_env := Envelope.new()
var lfo := LFO.new()
var distortion := Distortion.new()
var delay := TapeDelay.new()
var reverb := Reverb.new()

# ── State ──────────────────────────────────────────────────────
var _sample_rate: float = 44100.0
var _current_note: int = -1
var _current_freq: float = 0.0
var _target_freq: float = 0.0
var _is_sliding: bool = false
var _slide_samples_remaining: int = 0
var _slide_freq_step: float = 0.0

# ── Parameters (matches web defaults) ─────────────────────────
var waveform: SynthSpec.Waveform = SynthSpec.WAVEFORM_DEFAULT:
	set(v):
		waveform = v
		osc.waveform = v

var tuning: int = SynthSpec.TUNING_DEFAULT
var cutoff: float = SynthSpec.CUTOFF_DEFAULT
var resonance: float = SynthSpec.RESONANCE_DEFAULT
var env_mod: float = SynthSpec.ENVMOD_DEFAULT
var decay: float = SynthSpec.DECAY_DEFAULT
var accent: float = SynthSpec.ACCENT_DEFAULT

var lfo_rate: float = SynthSpec.LFO_RATE_DEFAULT
var lfo_amount: float = SynthSpec.LFO_AMOUNT_DEFAULT
var lfo_wave: SynthSpec.LfoWave = SynthSpec.LFO_WAVE_DEFAULT:
	set(v):
		lfo_wave = v
		lfo.wave = v

var dist_on: bool = false:
	set(v):
		dist_on = v
		distortion.enabled = v

var dist_amount: float = SynthSpec.DIST_AMOUNT_DEFAULT:
	set(v):
		dist_amount = v
		distortion.amount = v

var delay_on: bool = false:
	set(v):
		delay_on = v
		delay.enabled = v

var delay_time: float = SynthSpec.DELAY_TIME_DEFAULT:
	set(v):
		delay_time = v
		delay.delay_time = v

var delay_feedback: float = SynthSpec.DELAY_FEEDBACK_DEFAULT:
	set(v):
		delay_feedback = v
		delay.feedback = v

var delay_mix: float = SynthSpec.DELAY_MIX_DEFAULT:
	set(v):
		delay_mix = v
		delay.mix = v

var reverb_on: bool = false:
	set(v):
		reverb_on = v
		reverb.enabled = v

var reverb_decay: float = SynthSpec.REVERB_DECAY_DEFAULT:
	set(v):
		reverb_decay = v
		reverb.decay = v

var reverb_mix: float = SynthSpec.REVERB_MIX_DEFAULT:
	set(v):
		reverb_mix = v
		reverb.mix = v

# ── Audio output ───────────────────────────────────────────────
var _player: AudioStreamPlayer
var _playback: AudioStreamGeneratorPlayback
var _generator: AudioStreamGenerator


func _ready() -> void:
	_sample_rate = AudioServer.get_mix_rate()

	# Set up AudioStreamGenerator for per-sample DSP
	_generator = AudioStreamGenerator.new()
	_generator.mix_rate = _sample_rate
	_generator.buffer_length = 0.05  # 50ms buffer — low latency

	_player = AudioStreamPlayer.new()
	_player.stream = _generator
	_player.bus = "Master"
	add_child(_player)
	_player.play()
	_playback = _player.get_stream_playback()

	# Initialize effects that need sample rate
	delay.init(_sample_rate)
	reverb.init(_sample_rate)

	# Sub oscillator always square, one octave below
	sub_osc.waveform = SynthSpec.Waveform.SQUARE


func _process(_delta: float) -> void:
	_fill_buffer()


## Fill the generator buffer with audio samples.
func _fill_buffer() -> void:
	if _playback == null:
		return

	var frames := _playback.get_frames_available()
	if frames <= 0:
		return

	for i in range(frames):
		var sample := _generate_sample()
		# Mono synth → push same value to L and R
		_playback.push_frame(Vector2(sample, sample))


## Generate a single audio sample through the full signal chain.
func _generate_sample() -> float:
	# ── Slide handling ─────────────────────────────────────
	if _is_sliding and _slide_samples_remaining > 0:
		_current_freq += _slide_freq_step
		_slide_samples_remaining -= 1
		if _slide_samples_remaining <= 0:
			_current_freq = _target_freq
			_is_sliding = false

	# ── Oscillator ─────────────────────────────────────────
	var osc_out := osc.process_sample(_current_freq, _sample_rate)
	# Sub oscillator is silent for now (gain = 0 in web version)
	# var sub_out := sub_osc.process_sample(_current_freq / 2.0, _sample_rate)

	# ── LFO → filter detune ───────────────────────────────
	var lfo_val := lfo.process_sample(lfo_rate, _sample_rate)
	var detune_cents := lfo_val * lfo_amount
	var filter_freq := cutoff * pow(2.0, detune_cents / 1200.0)

	# ── Filter envelope ────────────────────────────────────
	var filt_env_val := filter_env.process_sample(decay, _sample_rate)
	filter_freq += filt_env_val

	# ── Diode ladder filter ────────────────────────────────
	var filtered := filter.process_sample(filter_freq, resonance, _sample_rate, osc_out)

	# ── VCA (amplitude envelope) ───────────────────────────
	var amp := amp_env.process_sample(decay, _sample_rate)
	var vca_out := filtered * amp

	# ── Effects chain ──────────────────────────────────────
	var sample := distortion.process_sample(vca_out)
	sample = delay.process_sample(sample, _sample_rate)
	sample = reverb.process_sample(sample)

	# ── Master volume ──────────────────────────────────────
	sample *= SynthSpec.MASTER_VOLUME

	# ── Soft clip (limiter equivalent) ─────────────────────
	sample = clampf(sample, -1.0, 1.0)

	return sample


# ── Note control API ───────────────────────────────────────────

## Trigger a note. Called by sequencer or keyboard input.
func trigger_note(midi: int, is_accent: bool = false, is_slide: bool = false) -> void:
	var freq := SynthSpec.midi_to_freq(midi + tuning)

	if is_slide and _current_note >= 0:
		# Portamento: glide to new frequency over SLIDE_TIME
		_target_freq = freq
		var slide_samples := int(SynthSpec.SLIDE_TIME * _sample_rate)
		_slide_freq_step = (_target_freq - _current_freq) / maxf(slide_samples, 1.0)
		_slide_samples_remaining = slide_samples
		_is_sliding = true
	else:
		_current_freq = freq
		_target_freq = freq
		_is_sliding = false
		_slide_samples_remaining = 0

	if not is_slide:
		# Amplitude envelope
		var accent_amt := accent if is_accent else 0.0
		var peak_vol := SynthSpec.BASE_VOL + accent_amt * 0.4
		var adj_decay := decay * (0.7 if is_accent else 1.0)
		amp_env.trigger(peak_vol, adj_decay, _sample_rate)

		# Filter envelope — store peak cutoff as the envelope value
		var env_depth := env_mod * (1.0 + accent_amt * 1.5)
		var peak_cutoff := minf(cutoff + env_depth, 12000.0)
		filter_env.trigger(peak_cutoff, adj_decay, _sample_rate)

	_current_note = midi


## Release the current note.
func release_note() -> void:
	amp_env.release(_sample_rate)
	_current_note = -1
