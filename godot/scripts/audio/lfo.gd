# ACID-303 Godot — LFO (Low Frequency Oscillator)
#
# Generates a low-frequency control signal used to modulate filter detune.
# Output is in the range [-1, +1] and gets scaled by lfo_amount (in cents).
class_name LFO


var _phase: float = 0.0
var wave: SynthSpec.LfoWave = SynthSpec.LFO_WAVE_DEFAULT


func reset() -> void:
	_phase = 0.0


## Generate one LFO sample. Returns value in [-1, +1].
func process_sample(rate: float, sample_rate: float) -> float:
	var out := 0.0
	match wave:
		SynthSpec.LfoWave.SINE:
			out = sin(TAU * _phase)
		SynthSpec.LfoWave.TRIANGLE:
			# Triangle: 0→1→0→-1→0 over one cycle
			if _phase < 0.25:
				out = _phase * 4.0
			elif _phase < 0.75:
				out = 2.0 - _phase * 4.0
			else:
				out = _phase * 4.0 - 4.0
		SynthSpec.LfoWave.SQUARE:
			out = 1.0 if _phase < 0.5 else -1.0

	_phase += rate / sample_rate
	_phase -= floorf(_phase)

	return out
