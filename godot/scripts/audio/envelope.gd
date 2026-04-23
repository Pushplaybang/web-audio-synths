# ACID-303 Godot — Envelope Generator
#
# Produces the amplitude and filter envelopes matching the web implementation.
# Uses exponential decay segments and tracks envelope phase per-sample.
class_name Envelope

enum Phase { OFF, ATTACK, DECAY, SUSTAIN, RELEASE }

var _phase: Phase = Phase.OFF
var _value: float = 0.0
var _target: float = 0.0
var _rate: float = 0.0  # per-sample multiplier or step


## Current envelope value (0–1+ range, can exceed 1 during accent peaks).
var value: float:
	get: return _value


## True when the envelope has finished and output is zero.
var is_off: bool:
	get: return _phase == Phase.OFF


func reset() -> void:
	_phase = Phase.OFF
	_value = 0.0
	_target = 0.0
	_rate = 0.0


## Trigger the envelope (note on). Caller provides peak and decay parameters.
##
## peak:       target amplitude at end of attack
## decay_time: time in seconds for decay phase
## sample_rate: audio sample rate
func trigger(peak: float, decay_time: float, sample_rate: float) -> void:
	_phase = Phase.ATTACK
	_target = peak
	# Linear attack over 5ms (AMP_ATTACK)
	var attack_samples := maxf(SynthSpec.AMP_ATTACK * sample_rate, 1.0)
	_rate = (peak - 0.001) / attack_samples
	_value = 0.001  # Start just above zero (exponential safety)


## Release the envelope (note off). Exponential approach to zero.
func release(sample_rate: float) -> void:
	if _phase == Phase.OFF:
		return
	_phase = Phase.RELEASE
	# Time constant of 10ms (matching web: setTargetAtTime tc=0.01)
	_rate = exp(-1.0 / (0.01 * sample_rate))
	_target = 0.0


## Advance envelope by one sample. Call once per audio sample.
## decay_time must be provided because it can change while the note plays.
func process_sample(decay_time: float, sample_rate: float) -> float:
	match _phase:
		Phase.OFF:
			_value = 0.0

		Phase.ATTACK:
			_value += _rate
			if _value >= _target:
				_value = _target
				# Transition to decay: exponential decay.
				# The 0.33 factor compresses the time constant so the decay
				# audibly reaches near-zero within the specified decay_time,
				# matching the web version's exponentialRampToValueAtTime feel.
				_phase = Phase.DECAY
				var sustain_level := SynthSpec.BASE_VOL * 0.4 + 0.001
				_rate = exp(-1.0 / (decay_time * sample_rate * 0.33))
				_target = sustain_level

		Phase.DECAY:
			_value = _value * _rate + _target * (1.0 - _rate)
			# Once close enough to zero, turn off
			if _value < 0.0001:
				_value = 0.0
				_phase = Phase.OFF

		Phase.RELEASE:
			_value *= _rate
			if _value < 0.0001:
				_value = 0.0
				_phase = Phase.OFF

	return _value
