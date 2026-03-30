# JS — Source Overview

This folder contains the JavaScript source for ACID-303, a monophonic acid synth emulator built entirely with the [Web Audio API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API).

## File Map

| File | Class / Exports | Purpose |
|---|---|---|
| [`synth.js`](synth.js) | `AcidSynth` | Audio engine — oscillators, filter, envelopes, effects, master chain |
| [`sequencer.js`](sequencer.js) | `Sequencer` | 16-step pattern sequencer with sample-accurate or fallback timing |
| [`ui.js`](ui.js) | `UI` | DOM interaction — knobs, toggles, step grid, keyboard, oscilloscope |
| [`worklets/`](worklets/) | AudioWorklet processors | Custom DSP running on the audio thread |

Detailed documentation for each file is linked below:

- [synth.md](synth.md) — AcidSynth class reference
- [sequencer.md](sequencer.md) — Sequencer class reference
- [ui.md](ui.md) — UI class reference
- [worklets/README.md](worklets/README.md) — AudioWorklet processors overview

## Architecture

### Boot Sequence

The application starts in `index.html` with three lines:

```js
const synth = new AcidSynth();
const seq   = new Sequencer(synth);
const ui    = new UI(synth, seq);
```

1. **`AcidSynth`** is created with default parameters but **no `AudioContext` yet** — browsers require a user gesture before creating one.
2. **`Sequencer`** receives a reference to the synth so it can trigger and release notes.
3. **`UI`** wires DOM events. The first user interaction (play, knob drag, key press) calls `synth.init()`, which creates the `AudioContext` and builds the full node graph.

### Signal Chain

```
Oscillator (saw / square) ──┐
                             ├──→ Filter ──→ VCA ──→ Distortion ──→ Delay ──→ Reverb
Sub-Oscillator (square) ────┘       ↑                                         │
                              LFO → filter.detune                             ▼
                                                                     Master Gain
                                                                         │
                                                                     Limiter
                                                                         │
                                                                     Analyser ──→ destination
```

Every node in the chain is a standard Web Audio [`AudioNode`](https://developer.mozilla.org/en-US/docs/Web/API/AudioNode). The filter is the one exception — it is either a custom `AudioWorkletNode` (diode ladder) or a built-in [`BiquadFilterNode`](https://developer.mozilla.org/en-US/docs/Web/API/BiquadFilterNode) fallback.

### Worklet vs Fallback

On `init()`, AcidSynth attempts to load two [AudioWorklet](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorklet) processors:

| Worklet | Fallback |
|---|---|
| `DiodeLadderProcessor` — 4-pole resonant lowpass with nonlinear saturation | `BiquadFilterNode` (lowpass) |
| `SeqClockProcessor` — sample-accurate sequencer clock | `setTimeout` lookahead scheduler |

If worklet loading fails (strict CSP, older browser), the fallback path activates transparently. A badge in the header shows `WORKLET` or `FALLBACK` so the user knows which engine is active.

The key design insight is that both paths expose the same [`AudioParam`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam) interface — `filterFreq`, `filterReso`, `filterDetune` — so all downstream code (envelopes, LFO, knob changes) works identically regardless of which implementation is running.

### Parameter Smoothing

All real-time parameter changes flow through a single helper:

```js
_smooth(param, value, timeConstant = 0.015)
```

This calls [`cancelScheduledValues`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam/cancelScheduledValues), anchors the current value with [`setValueAtTime`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam/setValueAtTime), then schedules a smooth exponential approach with [`setTargetAtTime`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam/setTargetAtTime). The result is click-free parameter changes on every knob, toggle, and internal state update.

### Effects Architecture

Each effect (distortion, delay, reverb) uses a parallel dry/wet topology:

```
Input ──→ Dry Gain ──→ Output Gain
  │                       ↑
  └──→ [Effect] ──→ Wet Gain
```

Toggling an effect on or off smoothly crossfades between dry and wet paths — no clicks or pops.

### Timing Model

The sequencer supports two timing strategies:

1. **Worklet clock** (preferred) — `SeqClockProcessor` counts samples on the audio thread and posts `{ step, time, stepDuration }` messages via [`MessagePort`](https://developer.mozilla.org/en-US/docs/Web/API/MessagePort). The `time` value is derived from the audio thread's `currentTime` parameter (the same [`AudioContext.currentTime`](https://developer.mozilla.org/en-US/docs/Web/API/BaseAudioContext/currentTime) observed at render-quantum boundaries) plus sample-level offsets within the block, giving sample-accurate scheduling.

2. **Fallback scheduler** — A `setTimeout` lookahead loop schedules notes 100 ms ahead using `AudioContext.currentTime`. This is the "Tale of Two Clocks" pattern — imprecise timer fires frequently, but actual note scheduling uses audio-thread time.

Both strategies call the same `_handleStep()` method, so the musical result is identical — only the timing precision differs.

## Key Web Audio Concepts Used

| Concept | MDN Reference |
|---|---|
| AudioContext lifecycle | [AudioContext](https://developer.mozilla.org/en-US/docs/Web/API/AudioContext) |
| Creating and connecting nodes | [AudioNode](https://developer.mozilla.org/en-US/docs/Web/API/AudioNode) |
| Scheduling parameter changes | [AudioParam](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam) |
| Custom DSP on the audio thread | [AudioWorklet](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorklet) |
| Oscillator types | [OscillatorNode](https://developer.mozilla.org/en-US/docs/Web/API/OscillatorNode) |
| Lowpass / resonant filter | [BiquadFilterNode](https://developer.mozilla.org/en-US/docs/Web/API/BiquadFilterNode) |
| Gain control and VCA | [GainNode](https://developer.mozilla.org/en-US/docs/Web/API/GainNode) |
| Waveshaping distortion | [WaveShaperNode](https://developer.mozilla.org/en-US/docs/Web/API/WaveShaperNode) |
| Convolution reverb | [ConvolverNode](https://developer.mozilla.org/en-US/docs/Web/API/ConvolverNode) |
| Dynamics processing / limiting | [DynamicsCompressorNode](https://developer.mozilla.org/en-US/docs/Web/API/DynamicsCompressorNode) |
| FFT / waveform analysis | [AnalyserNode](https://developer.mozilla.org/en-US/docs/Web/API/AnalyserNode) |
| Delay lines | [DelayNode](https://developer.mozilla.org/en-US/docs/Web/API/DelayNode) |
