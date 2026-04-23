# ACID-303 Godot — Distortion Effect
#
# Waveshaper distortion matching the web implementation's transfer function.
# f(x) = ((1 + k) * x) / (1 + k * abs(x)) where k = amount * 2
class_name Distortion


var enabled: bool = false
var amount: float = SynthSpec.DIST_AMOUNT_DEFAULT


## Process a single sample through the waveshaper.
func process_sample(input: float) -> float:
	if not enabled:
		return input
	var k := amount * 2.0
	if k < 0.001:
		return input
	return ((1.0 + k) * input) / (1.0 + k * absf(input))
