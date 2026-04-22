# ACID-303 Godot — Reverb Effect
#
# Simple algorithmic reverb using a feedback delay network.
# The web version uses convolution with generated noise IR — here we use
# a lightweight FDN that gives a similar character without needing FFT.
class_name Reverb


var enabled: bool = false
var decay: float = SynthSpec.REVERB_DECAY_DEFAULT
var mix: float = SynthSpec.REVERB_MIX_DEFAULT

# Four delay lines with prime-ish lengths for diffusion
const DELAY_LENGTHS := [1557, 1617, 1491, 1422]
const NUM_LINES := 4

var _buffers: Array[PackedFloat32Array] = []
var _write_positions: PackedInt32Array
var _sample_rate: float = 44100.0


func init(sample_rate: float) -> void:
	_sample_rate = sample_rate
	_buffers.clear()
	_write_positions = PackedInt32Array()
	_write_positions.resize(NUM_LINES)

	for i in range(NUM_LINES):
		var buf := PackedFloat32Array()
		buf.resize(DELAY_LENGTHS[i])
		buf.fill(0.0)
		_buffers.append(buf)
		_write_positions[i] = 0


func reset() -> void:
	for buf in _buffers:
		buf.fill(0.0)


## Process one sample through the reverb.
func process_sample(input: float) -> float:
	if not enabled or _buffers.is_empty():
		return input

	# Feedback coefficient derived from decay time
	# For a 1500-sample delay at 44100 Hz, we want amplitude to reach
	# -60 dB after 'decay' seconds.
	var avg_delay := 1521.75  # average of DELAY_LENGTHS
	var loop_time := avg_delay / _sample_rate
	var fb := pow(0.001, loop_time / maxf(decay, 0.1))

	var wet := 0.0
	for i in range(NUM_LINES):
		var buf := _buffers[i]
		var size := buf.size()
		var read_pos := (_write_positions[i] + 1) % size
		var delayed := buf[read_pos]
		wet += delayed

		# Write input + feedback
		buf[_write_positions[i]] = input * 0.25 + delayed * fb
		_write_positions[i] = (_write_positions[i] + 1) % size

	wet *= 0.25  # Average the 4 lines

	# Dry/wet mix
	var dry_level := 1.0 - mix * 0.3  # Matching web: reverbDry = 1 - mix * 0.3
	return input * dry_level + wet * mix
