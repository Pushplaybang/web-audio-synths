# ACID-303 Godot — Tape Delay Effect
#
# Simple feedback delay with a lowpass filter in the feedback path,
# matching the web implementation.
class_name TapeDelay


var enabled: bool = false
var delay_time: float = SynthSpec.DELAY_TIME_DEFAULT
var feedback: float = SynthSpec.DELAY_FEEDBACK_DEFAULT
var mix: float = SynthSpec.DELAY_MIX_DEFAULT

var _buffer: PackedFloat32Array
var _buffer_size: int = 0
var _write_pos: int = 0
var _filter_state: float = 0.0

# Feedback filter: one-pole lowpass at 3500 Hz
const FEEDBACK_FILTER_FREQ := 3500.0


func init(sample_rate: float, max_delay: float = 2.0) -> void:
	_buffer_size = int(sample_rate * max_delay)
	_buffer = PackedFloat32Array()
	_buffer.resize(_buffer_size)
	_buffer.fill(0.0)
	_write_pos = 0
	_filter_state = 0.0


func reset() -> void:
	if _buffer_size > 0:
		_buffer.fill(0.0)
	_write_pos = 0
	_filter_state = 0.0


## Process a single sample through the delay.
func process_sample(input: float, sample_rate: float) -> float:
	if not enabled or _buffer_size == 0:
		return input

	# Read from delay buffer
	var delay_samples := int(delay_time * sample_rate)
	delay_samples = clampi(delay_samples, 1, _buffer_size - 1)
	var read_pos := (_write_pos - delay_samples + _buffer_size) % _buffer_size
	var delayed := _buffer[read_pos]

	# One-pole lowpass on feedback path (matching 3500 Hz in web version)
	var lp_coeff := 1.0 - exp(-TAU * FEEDBACK_FILTER_FREQ / sample_rate)
	_filter_state += lp_coeff * (delayed - _filter_state)

	# Write input + filtered feedback to buffer
	_buffer[_write_pos] = input + _filter_state * feedback
	_write_pos = (_write_pos + 1) % _buffer_size

	# Wet/dry mix
	return input * (1.0 - mix) + delayed * mix
