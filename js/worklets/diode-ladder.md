# diode-ladder.js — DiodeLadderProcessor

`DiodeLadderProcessor` is an [`AudioWorkletProcessor`](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletProcessor) that implements a 4-pole (24 dB/octave) resonant lowpass filter modeled on the transistor ladder topology found in the Roland TB-303.

## Table of Contents

- [Why a Custom Filter?](#why-a-custom-filter)
- [AudioParam Interface](#audioparam-interface)
- [Algorithm](#algorithm)
  - [Frequency Warping](#frequency-warping)
  - [Feedback and Resonance](#feedback-and-resonance)
  - [Four Cascaded Stages](#four-cascaded-stages)
  - [Nonlinear Saturation](#nonlinear-saturation)
- [Internal State](#internal-state)
- [Numerical Stability](#numerical-stability)
- [Registration](#registration)

---

## Why a Custom Filter?

The standard [`BiquadFilterNode`](https://developer.mozilla.org/en-US/docs/Web/API/BiquadFilterNode) implements a textbook linear lowpass filter. It works correctly, but it sounds "clean" — the transfer function is perfectly linear, so the output is always a scaled/phase-shifted version of the input.

The TB-303's filter is a transistor ladder circuit where each stage introduces mild saturation (soft clipping). At low signal levels the behavior is nearly linear, but as resonance increases and signal levels rise, the saturation per stage becomes audible:

- Harmonics are added to the signal at each stage.
- The resonance peak is naturally tamed (self-limiting) — unlike a linear filter where high Q values can produce runaway oscillation.
- The filter has a warm, compressed, "squelchy" character that is the defining sound of acid music.

No combination of standard Web Audio nodes can replicate this behavior. The nonlinearity must be computed per-sample, which is exactly what AudioWorklet provides.

---

## AudioParam Interface

Declared in [`parameterDescriptors`](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletProcessor/parameterDescriptors):

| Parameter | Default | Min | Max | Rate | Description |
|---|---|---|---|---|---|
| `frequency` | 800 | 20 | 18000 | a-rate | Cutoff frequency in Hz |
| `resonance` | 8 | 0 | 30 | a-rate | Resonance amount (mapped to feedback gain 0–3.8) |
| `detune` | 0 | −4800 | 4800 | a-rate | Detuning in cents (±4 octaves), used for LFO modulation |

All three parameters support [a-rate automation](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam#a-rate), meaning they can change per sample within a 128-sample block. This is important for:

- **Frequency** — Filter envelope sweeps that span the full range in a few milliseconds.
- **Detune** — LFO modulation that needs to be sample-accurate to avoid zipper noise.

---

## Algorithm

The filter processes audio one sample at a time inside the `process()` method. For each sample `i` in the 128-sample block:

### Frequency Warping

```js
let freq = frequency[i] * Math.pow(2, detune[i] / 1200);
if (freq < 30) freq = 30;
if (freq > ny) freq = ny;  // ny = sampleRate * 0.45
const g = 2 * Math.tan(Math.PI * freq / sampleRate);
const G = g / (1 + g);
```

1. **Apply detuning** — Detune in cents is converted to a frequency multiplier using the formula `2^(cents/1200)`. This is the same formula used by the Web Audio spec for [`AudioParam.detune`](https://developer.mozilla.org/en-US/docs/Web/API/OscillatorNode/detune).
2. **Clamp** — The effective frequency is clamped between 30 Hz (to prevent DC or sub-audio instability) and 45% of the sample rate (`sampleRate * 0.45`). This is intentionally lower than the true Nyquist frequency (50%) to provide a safety margin against aliasing in the nonlinear saturation stages.
3. **Bilinear transform** — `g = 2 * tan(π * f / sr)` maps the analog prototype frequency to the digital domain. This is the same transform used in standard IIR filter design, and it preserves the frequency response shape at the expense of slight frequency warping near Nyquist.
4. **Integrator gain** — `G = g / (1 + g)` is the one-pole integrator transfer coefficient used in the Topology-Preserving Transform (TPT) form of the filter.

### Feedback and Resonance

```js
const k = (resonance[i] / 30) * 3.8;
const fb = k * this.t(this.s[3]);
```

- The resonance parameter (0–30) is linearly mapped to a feedback coefficient `k` in the range 0–3.8. At `k = 4` the filter would self-oscillate; 3.8 keeps it just below that threshold.
- The feedback signal is taken from the **output of the fourth stage** (`this.s[3]`) — this is the classic ladder topology where the output feeds back to the input.
- The feedback signal passes through the saturation function `t()` before being subtracted from the input. This nonlinear feedback is what gives the 303 filter its self-limiting resonance character.

### Four Cascaded Stages

```js
let x = this.t(input[i] - fb);
this.s[0] += G * (x - this.s[0]); x = this.t(this.s[0]);
this.s[1] += G * (x - this.s[1]); x = this.t(this.s[1]);
this.s[2] += G * (x - this.s[2]); x = this.t(this.s[2]);
this.s[3] += G * (x - this.s[3]);
output[i] = this.s[3];
```

Each stage is a first-order lowpass filter (one-pole integrator) in TPT form:

```
state += G * (input - state)
```

This is equivalent to a one-pole IIR filter but written in a way that is numerically stable and easy to modulate. Four stages in series create a 4-pole (24 dB/octave) rolloff.

Between each stage, the signal passes through the saturation function `t()`. This inter-stage saturation is what differentiates the diode ladder from a linear 4-pole filter.

### Nonlinear Saturation

```js
t(x) {
  if (x > 3) return 1;
  if (x < -3) return -1;
  const x2 = x * x;
  return x * (27 + x2) / (27 + 9 * x2);
}
```

This is a rational approximation of `tanh(x)` that is cheaper to compute than the transcendental function but has the same essential shape:

- **Linear region** around zero — small signals pass through unchanged.
- **Soft saturation** — as the signal magnitude increases, the output curves toward ±1.
- **Hard clip at ±3** — extreme values are clamped for safety.

The approximation `x(27 + x²) / (27 + 9x²)` matches `tanh(x)` to within a few percent over the range [−3, 3], which is more than adequate for an audio effect.

---

## Internal State

```js
this.s = new Float64Array(4);
```

The four filter stages each maintain a single state variable (`s[0]` through `s[3]`). These are the "memory" of the filter — they hold the integrated signal value from the previous sample.

`Float64Array` (64-bit doubles) is used instead of `Float32Array` for **numerical stability**. Filter state variables accumulate over millions of samples, and the small rounding errors in 32-bit floats can compound into audible artifacts, especially at low cutoff frequencies where the integrator gain `G` is very small.

---

## Numerical Stability

Several design choices ensure the filter remains stable under all parameter combinations:

1. **Frequency clamping** — Never below 30 Hz or above 0.45 × sample rate.
2. **Resonance ceiling** — `k` maxes out at 3.8, staying below the self-oscillation threshold of 4.0.
3. **Saturation limiting** — The `t()` function hard-clips at ±1, preventing runaway values even if input is very hot.
4. **Float64 state** — 64-bit precision prevents accumulated rounding errors from producing DC drift or noise.
5. **TPT topology** — The Topology-Preserving Transform form avoids the coefficient sensitivity issues of direct-form IIR implementations.

---

## Registration

```js
registerProcessor('diode-ladder', DiodeLadderProcessor);
```

This makes the processor available to the main thread via:

```js
new AudioWorkletNode(ctx, 'diode-ladder', { ... });
```

See the MDN documentation on [`registerProcessor`](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletGlobalScope/registerProcessor) for details.
