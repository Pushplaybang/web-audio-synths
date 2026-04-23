# ACID-303 Godot — Oscillator
#
# Band-limited oscillator using naive waveform generation.
# Produces sawtooth or square at a given frequency.
# Phase-continuous across frequency changes for glitch-free slides.
class_name Oscillator


var _phase: float = 0.0
var waveform: SynthSpec.Waveform = SynthSpec.WAVEFORM_DEFAULT


func reset() -> void:
	_phase = 0.0


## Generate one sample at the given frequency.
func process_sample(freq: float, sample_rate: float) -> float:
	var out := 0.0
	match waveform:
		SynthSpec.Waveform.SAW:
			# Sawtooth: ramp from -1 to +1
			out = 2.0 * _phase - 1.0
		SynthSpec.Waveform.SQUARE:
			# Square: +1 / -1
			out = 1.0 if _phase < 0.5 else -1.0

	# Advance phase, wrapping at 1.0
	_phase += freq / sample_rate
	_phase -= floorf(_phase)

	return out
