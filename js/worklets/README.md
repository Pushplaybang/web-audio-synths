# worklets/ — AudioWorklet Processors

This folder contains two [AudioWorklet](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorklet) processors that run on the **audio thread**, providing custom DSP and sample-accurate timing that are impossible to achieve on the main thread.

## What is AudioWorklet?

[AudioWorklet](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorklet) is a Web Audio API feature that allows custom audio processing code to run directly on the audio rendering thread. This is critical for two reasons:

1. **Custom DSP** — Standard Web Audio nodes (e.g., [`BiquadFilterNode`](https://developer.mozilla.org/en-US/docs/Web/API/BiquadFilterNode)) are limited to their built-in algorithms. AudioWorklet allows per-sample math with arbitrary behavior — in this project, a nonlinear diode ladder filter that standard nodes cannot replicate.

2. **Sample-accurate timing** — The audio thread processes samples in blocks of 128 at a precise, deterministic rate. A worklet processor can count samples and derive timing information that is far more accurate than any main-thread timer.

### How It Works

Each processor is a class extending [`AudioWorkletProcessor`](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletProcessor) with a `process()` method that runs once per audio block (128 samples). The processor is loaded from a JavaScript file into the audio thread via [`audioWorklet.addModule()`](https://developer.mozilla.org/en-US/docs/Web/API/Worklet/addModule), then instantiated on the main thread as an [`AudioWorkletNode`](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletNode).

Communication between the main thread and the audio thread happens through:

- **[`AudioParam`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam)** — For continuously changing values (frequency, tempo). Declared in `parameterDescriptors` and accessible from both threads.
- **[`MessagePort`](https://developer.mozilla.org/en-US/docs/Web/API/MessagePort)** — For discrete events (step triggers, reset commands). Available as `this.port` inside the processor and `node.port` on the main thread.

## Files

| File | Processor Name | Purpose |
|---|---|---|
| [`diode-ladder.js`](diode-ladder.js) | `diode-ladder` | 4-pole resonant lowpass filter with nonlinear saturation |
| [`seq-clock.js`](seq-clock.js) | `seq-clock` | Sample-counting sequencer clock |

Detailed documentation:

- [diode-ladder.md](diode-ladder.md) — Filter processor reference
- [seq-clock.md](seq-clock.md) — Clock processor reference

## Fallback Behavior

Both processors have main-thread fallbacks in case AudioWorklet is unavailable (strict Content Security Policy, older browsers):

| Worklet | Fallback |
|---|---|
| `DiodeLadderProcessor` | [`BiquadFilterNode`](https://developer.mozilla.org/en-US/docs/Web/API/BiquadFilterNode) set to `'lowpass'` |
| `SeqClockProcessor` | `setTimeout` lookahead scheduler |

The synth engine detects the failure in `AcidSynth.init()` and selects the fallback path. All downstream code uses the same [`AudioParam`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam) interface regardless of which implementation is active.

## Loading

The processors are loaded from file URLs resolved relative to the document:

```js
const base = new URL('.', document.baseURI).href;
await ctx.audioWorklet.addModule(new URL('js/worklets/diode-ladder.js', base).href);
await ctx.audioWorklet.addModule(new URL('js/worklets/seq-clock.js', base).href);
```

This approach works with both local `file://` serving and deployed environments like GitHub Pages.
