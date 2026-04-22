# ACID-303 Godot — 4-Pole Diode Ladder Filter
#
# Direct port of js/worklets/diode-ladder.js.
# Runs per-sample inside the audio callback via AudioStreamGenerator.
# Uses float64 (GDScript default) for numerical stability.
class_name DiodeLadderFilter


var _state: PackedFloat64Array  # 4-pole filter state


func _init() -> void:
	_state = PackedFloat64Array()
	_state.resize(4)
	reset()


func reset() -> void:
	for i in range(4):
		_state[i] = 0.0


## tanh approximation matching the JS implementation.
## Hard-clamps outside [-3, 3] for efficiency; polynomial inside.
func _tanh_approx(x: float) -> float:
	if x > 3.0:
		return 1.0
	if x < -3.0:
		return -1.0
	var x2 := x * x
	return x * (27.0 + x2) / (27.0 + 9.0 * x2)


## Process a single sample through the 4-pole ladder.
##
## freq:       filter cutoff in Hz (already includes detune)
## resonance:  0–30 range (same as web spec)
## sample_rate: audio server sample rate
## input:      input sample value
##
## Returns: filtered output sample
func process_sample(freq: float, resonance: float, sample_rate: float, input: float) -> float:
	# Clamp frequency to safe range
	var nyquist := sample_rate * 0.45
	freq = clampf(freq, SynthSpec.FILTER_FREQ_FLOOR, nyquist)

	# Bilinear pre-warp coefficient
	var g := 2.0 * tan(PI * freq / sample_rate)
	var big_g := g / (1.0 + g)

	# Resonance feedback coefficient: map 0–30 to 0–3.8
	var k := (resonance / 30.0) * SynthSpec.FILTER_RESONANCE_SCALE

	# Feedback from last stage
	var fb := k * _tanh_approx(_state[3])

	# Input with feedback subtracted, saturated
	var x := _tanh_approx(input - fb)

	# Four cascaded one-pole stages with per-stage saturation
	_state[0] += big_g * (x - _state[0])
	x = _tanh_approx(_state[0])

	_state[1] += big_g * (x - _state[1])
	x = _tanh_approx(_state[1])

	_state[2] += big_g * (x - _state[2])
	x = _tanh_approx(_state[2])

	_state[3] += big_g * (x - _state[3])

	return _state[3]
