# ACID-303 Godot — Main Scene Script
#
# Wires together the synth engine, sequencer, and UI.
# Attached to the root node of main.tscn.
extends Control


@onready var synth_engine: AcidSynthEngine = $AcidSynthEngine
@onready var sequencer: StepSequencer = $StepSequencer

# ── Transport ──────────────────────────────────────────────────
@onready var play_btn: Button = %PlayBtn
@onready var stop_btn: Button = %StopBtn
@onready var random_btn: Button = %RandomBtn
@onready var tempo_display: Label = %TempoDisplay
@onready var tempo_up: Button = %TempoUp
@onready var tempo_down: Button = %TempoDown

# ── Oscillator ─────────────────────────────────────────────────
@onready var wave_saw_btn: Button = %WaveSaw
@onready var wave_square_btn: Button = %WaveSquare
@onready var tuning_slider: HSlider = %TuningSlider

# ── Filter ─────────────────────────────────────────────────────
@onready var cutoff_slider: HSlider = %CutoffSlider
@onready var reso_slider: HSlider = %ResoSlider
@onready var envmod_slider: HSlider = %EnvModSlider

# ── Envelope ───────────────────────────────────────────────────
@onready var decay_slider: HSlider = %DecaySlider
@onready var accent_slider: HSlider = %AccentSlider

# ── Step grid ──────────────────────────────────────────────────
@onready var step_grid: GridContainer = %StepGrid
@onready var note_grid: GridContainer = %NoteGrid

var _step_buttons: Array[Button] = []
var _note_labels: Array[Label] = []
var _edit_mode: String = "gate"
var _selected_step: int = -1


func _ready() -> void:
	sequencer.set_synth(synth_engine)
	_connect_transport()
	_connect_oscillator()
	_connect_filter()
	_connect_envelope()
	_build_step_grid()
	_update_tempo_display()
	sequencer.step_changed.connect(_on_step_changed)


func _connect_transport() -> void:
	play_btn.pressed.connect(_on_play)
	stop_btn.pressed.connect(_on_stop)
	random_btn.pressed.connect(_on_random)
	tempo_up.pressed.connect(func(): _change_tempo(2))
	tempo_down.pressed.connect(func(): _change_tempo(-2))


func _connect_oscillator() -> void:
	wave_saw_btn.pressed.connect(func():
		synth_engine.waveform = SynthSpec.Waveform.SAW
		wave_saw_btn.button_pressed = true
		wave_square_btn.button_pressed = false
	)
	wave_square_btn.pressed.connect(func():
		synth_engine.waveform = SynthSpec.Waveform.SQUARE
		wave_saw_btn.button_pressed = false
		wave_square_btn.button_pressed = true
	)
	if tuning_slider:
		tuning_slider.value = SynthSpec.TUNING_DEFAULT
		tuning_slider.value_changed.connect(func(v: float): synth_engine.tuning = int(v))


func _connect_filter() -> void:
	if cutoff_slider:
		cutoff_slider.min_value = SynthSpec.CUTOFF_MIN
		cutoff_slider.max_value = SynthSpec.CUTOFF_MAX
		cutoff_slider.value = SynthSpec.CUTOFF_DEFAULT
		cutoff_slider.value_changed.connect(func(v: float): synth_engine.cutoff = v)
	if reso_slider:
		reso_slider.min_value = SynthSpec.RESONANCE_MIN
		reso_slider.max_value = SynthSpec.RESONANCE_MAX
		reso_slider.value = SynthSpec.RESONANCE_DEFAULT
		reso_slider.value_changed.connect(func(v: float): synth_engine.resonance = v)
	if envmod_slider:
		envmod_slider.min_value = SynthSpec.ENVMOD_MIN
		envmod_slider.max_value = SynthSpec.ENVMOD_MAX
		envmod_slider.value = SynthSpec.ENVMOD_DEFAULT
		envmod_slider.value_changed.connect(func(v: float): synth_engine.env_mod = v)


func _connect_envelope() -> void:
	if decay_slider:
		decay_slider.min_value = SynthSpec.DECAY_MIN
		decay_slider.max_value = SynthSpec.DECAY_MAX
		decay_slider.value = SynthSpec.DECAY_DEFAULT
		decay_slider.value_changed.connect(func(v: float): synth_engine.decay = v)
	if accent_slider:
		accent_slider.min_value = SynthSpec.ACCENT_MIN
		accent_slider.max_value = SynthSpec.ACCENT_MAX
		accent_slider.value = SynthSpec.ACCENT_DEFAULT
		accent_slider.value_changed.connect(func(v: float): synth_engine.accent = v)


# ── Step grid ──────────────────────────────────────────────────

func _build_step_grid() -> void:
	if not step_grid or not note_grid:
		return

	_step_buttons.clear()
	_note_labels.clear()

	for child in step_grid.get_children():
		child.queue_free()
	for child in note_grid.get_children():
		child.queue_free()

	for i in range(SynthSpec.SEQ_STEPS):
		# Step button
		var btn := Button.new()
		btn.custom_minimum_size = Vector2(40, 40)
		btn.toggle_mode = true
		btn.button_pressed = sequencer.steps[i].gate
		btn.pressed.connect(_on_step_toggled.bind(i))
		step_grid.add_child(btn)
		_step_buttons.append(btn)

		# Note label
		var lbl := Label.new()
		lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		lbl.custom_minimum_size = Vector2(40, 20)
		lbl.text = SynthSpec.note_to_name(sequencer.steps[i].note)
		note_grid.add_child(lbl)
		_note_labels.append(lbl)

	_refresh_step_grid()


func _refresh_step_grid() -> void:
	for i in range(SynthSpec.SEQ_STEPS):
		if i >= _step_buttons.size():
			break
		var s := sequencer.steps[i]
		var btn := _step_buttons[i]
		match _edit_mode:
			"gate":
				btn.button_pressed = s.gate
			"accent":
				btn.button_pressed = s.accent
			"slide":
				btn.button_pressed = s.slide
		_note_labels[i].text = SynthSpec.note_to_name(s.note)


func _on_step_toggled(step_idx: int) -> void:
	var s := sequencer.steps[step_idx]
	match _edit_mode:
		"gate":
			s.gate = not s.gate
		"accent":
			s.accent = not s.accent
		"slide":
			s.slide = not s.slide
	_refresh_step_grid()


func _on_step_changed(step: int) -> void:
	for i in range(_step_buttons.size()):
		var btn := _step_buttons[i]
		# Visual highlight for current step
		if i == step:
			btn.add_theme_color_override("font_color", Color.ORANGE_RED)
		else:
			btn.remove_theme_color_override("font_color")


# ── Transport handlers ─────────────────────────────────────────

func _on_play() -> void:
	sequencer.start()
	play_btn.disabled = true
	stop_btn.disabled = false


func _on_stop() -> void:
	sequencer.stop()
	play_btn.disabled = false
	stop_btn.disabled = true
	for btn in _step_buttons:
		btn.remove_theme_color_override("font_color")


func _on_random() -> void:
	sequencer.randomize_pattern()
	_refresh_step_grid()


func _change_tempo(delta: int) -> void:
	sequencer.tempo = clampi(sequencer.tempo + delta, SynthSpec.TEMPO_MIN, SynthSpec.TEMPO_MAX)
	_update_tempo_display()


func _update_tempo_display() -> void:
	if tempo_display:
		tempo_display.text = str(sequencer.tempo)


# ── Keyboard input ─────────────────────────────────────────────

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		_handle_key(event)


func _handle_key(event: InputEventKey) -> void:
	var key := event.keycode

	# Space: toggle play/stop
	if key == KEY_SPACE:
		if sequencer.is_playing:
			_on_stop()
		else:
			_on_play()
		get_viewport().set_input_as_handled()
		return

	# Arrow keys: adjust selected step note
	if _selected_step >= 0:
		if key == KEY_UP:
			sequencer.steps[_selected_step].note = mini(SynthSpec.NOTE_MAX, sequencer.steps[_selected_step].note + 1)
			_refresh_step_grid()
			get_viewport().set_input_as_handled()
			return
		if key == KEY_DOWN:
			sequencer.steps[_selected_step].note = maxi(SynthSpec.NOTE_MIN, sequencer.steps[_selected_step].note - 1)
			_refresh_step_grid()
			get_viewport().set_input_as_handled()
			return

	# Musical keyboard
	var note_offset := _key_to_note(key)
	if note_offset >= 0:
		var midi := SynthSpec.KEYBOARD_BASE_OCTAVE + note_offset
		if _selected_step >= 0:
			sequencer.steps[_selected_step].note = midi
			sequencer.steps[_selected_step].gate = true
			_selected_step = (_selected_step + 1) % SynthSpec.SEQ_STEPS
			_refresh_step_grid()
		synth_engine.trigger_note(midi)
		get_viewport().set_input_as_handled()


func _key_to_note(keycode: int) -> int:
	match keycode:
		KEY_Z: return 0
		KEY_S: return 1
		KEY_X: return 2
		KEY_D: return 3
		KEY_C: return 4
		KEY_V: return 5
		KEY_G: return 6
		KEY_B: return 7
		KEY_H: return 8
		KEY_N: return 9
		KEY_J: return 10
		KEY_M: return 11
	return -1
