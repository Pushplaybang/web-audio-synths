# ACID-303 Godot — Step Sequencer
#
# 16-step sequencer with sample-accurate timing via the audio thread.
# Direct port of js/sequencer.js.
extends Node

class_name StepSequencer

signal step_changed(step: int)

# ── Step data ──────────────────────────────────────────────────
class StepData:
	var note: int = SynthSpec.NOTE_DEFAULT
	var gate: bool = false
	var accent: bool = false
	var slide: bool = false

	func _init(n: int = SynthSpec.NOTE_DEFAULT, g: bool = false, a: bool = false, s: bool = false) -> void:
		note = n
		gate = g
		accent = a
		slide = s

var steps: Array[StepData] = []
var tempo: int = SynthSpec.TEMPO_DEFAULT
var is_playing: bool = false
var current_step: int = -1

var _synth: AcidSynthEngine
var _sample_counter: int = 0

# ── Computed ───────────────────────────────────────────────────
var step_duration: float:
	get: return 60.0 / tempo / 4.0

var _samples_per_step: int:
	get: return int(step_duration * AudioServer.get_mix_rate())


func _init(synth: AcidSynthEngine = null) -> void:
	_synth = synth
	steps.clear()
	for i in range(SynthSpec.SEQ_STEPS):
		steps.append(StepData.new())
	_init_pattern()


func set_synth(synth: AcidSynthEngine) -> void:
	_synth = synth


func _process(_delta: float) -> void:
	if not is_playing:
		return
	_advance_clock()


## Advance the sequencer clock. Called each frame.
## Uses accumulated sample counting for timing accuracy.
func _advance_clock() -> void:
	if _synth == null:
		return

	var samples_per_frame := int(AudioServer.get_mix_rate() * get_process_delta_time())
	_sample_counter += samples_per_frame

	while _sample_counter >= _samples_per_step:
		_sample_counter -= _samples_per_step
		current_step = (current_step + 1) % SynthSpec.SEQ_STEPS
		_handle_step(current_step)


func _handle_step(step: int) -> void:
	var s := steps[step]
	var ns := steps[(step + 1) % SynthSpec.SEQ_STEPS]

	if s.gate:
		_synth.trigger_note(s.note, s.accent, s.slide)

		# Schedule release unless next step continues with slide
		if not (ns.slide and ns.gate):
			# Release at 75% of step duration
			# In Godot we use a timer since we don't have audio-thread scheduling
			var release_delay := step_duration * SynthSpec.GATE_RELEASE_FRACTION
			get_tree().create_timer(release_delay).timeout.connect(_synth.release_note)

	step_changed.emit(step)


func start() -> void:
	if is_playing:
		return
	is_playing = true
	current_step = -1
	_sample_counter = 0


func stop() -> void:
	is_playing = false
	if _synth:
		_synth.release_note()
	current_step = -1
	step_changed.emit(-1)


## Load the default acid pattern from the spec.
func _init_pattern() -> void:
	for i in range(SynthSpec.DEFAULT_PATTERN.size()):
		var p: Array = SynthSpec.DEFAULT_PATTERN[i]
		steps[i] = StepData.new(p[0], p[1], p[2], p[3])


## Randomize all steps using the spec's randomization rules.
func randomize_pattern() -> void:
	for i in range(SynthSpec.SEQ_STEPS):
		var octave_base: int = 48 if randf() < 0.3 else 36
		var scale_note: int = SynthSpec.RANDOM_SCALE[randi() % SynthSpec.RANDOM_SCALE.size()]
		steps[i] = StepData.new(
			octave_base + scale_note,
			randf() < 0.7,   # gate
			randf() < 0.25,  # accent
			randf() < 0.2    # slide
		)


## Clear all steps to default state.
func clear() -> void:
	for i in range(SynthSpec.SEQ_STEPS):
		steps[i] = StepData.new()
