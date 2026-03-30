# synth.js — AcidSynth Class

`AcidSynth` is the audio engine for ACID-303. It owns the [`AudioContext`](https://developer.mozilla.org/en-US/docs/Web/API/AudioContext), builds the entire node graph, and provides methods to trigger notes, release notes, and change parameters in real time.

## Table of Contents

- [Construction](#construction)
- [Initialization](#initialization)
- [Signal Chain](#signal-chain)
  - [Oscillators](#oscillators)
  - [Filter](#filter)
  - [LFO](#lfo)
  - [VCA](#vca)
  - [Distortion](#distortion)
  - [Delay](#delay)
  - [Reverb](#reverb)
  - [Master and Limiter](#master-and-limiter)
  - [Analyser](#analyser)
- [Note Triggering](#note-triggering)
- [Note Release](#note-release)
- [Parameter Control](#parameter-control)
- [Oscilloscope Toggle](#oscilloscope-toggle)
- [Helper Utilities](#helper-utilities)

---

## Construction

```js
const synth = new AcidSynth();
```

The constructor sets default parameters and initializes state but **does not create an `AudioContext`**. Browsers require a user gesture before audio can start, so the context is created lazily in `init()`.

### Default Parameters

| Parameter | Default | Description |
|---|---|---|
| `waveform` | `'sawtooth'` | Oscillator wave shape |
| `tuning` | `0` | Semitone offset applied to incoming MIDI notes |
| `cutoff` | `800` | Filter base frequency in Hz |
| `resonance` | `8` | Filter resonance (Q for fallback, custom scale for worklet) |
| `envMod` | `3000` | Filter envelope modulation depth in Hz |
| `decay` | `0.3` | Envelope decay time in seconds |
| `accent` | `0.6` | Accent intensity (0–1) |
| `distOn` | `false` | Distortion bypass state |
| `distAmount` | `40` | Distortion drive amount |
| `delayOn` | `false` | Delay bypass state |
| `delayTime` | `0.375` | Delay time in seconds |
| `delayFeedback` | `0.45` | Delay feedback gain (0–0.9) |
| `delayMix` | `0.3` | Delay wet/dry mix |
| `reverbOn` | `false` | Reverb bypass state |
| `reverbDecay` | `2.0` | Reverb tail length in seconds |
| `reverbMix` | `0.2` | Reverb wet/dry mix |
| `lfoRate` | `4` | LFO frequency in Hz |
| `lfoAmount` | `0` | LFO modulation depth in cents |
| `lfoWave` | `'sine'` | LFO waveform |

---

## Initialization

```js
await synth.init();
```

`init()` is async and idempotent — calling it multiple times is safe. It performs these steps:

1. **Creates the `AudioContext`** via [`new AudioContext()`](https://developer.mozilla.org/en-US/docs/Web/API/AudioContext/AudioContext).
2. **Loads AudioWorklet modules** — Resolves paths relative to `document.baseURI` and calls [`audioWorklet.addModule()`](https://developer.mozilla.org/en-US/docs/Web/API/Worklet/addModule) for both the diode ladder filter and the sequencer clock. If loading fails, sets `workletAvailable = false`.
3. **Updates the engine badge** in the DOM to show `WORKLET` or `FALLBACK`.
4. **Builds the node graph** — creates every `AudioNode`, sets initial values, and connects the signal chain.
5. **Starts oscillators** — calls [`start()`](https://developer.mozilla.org/en-US/docs/Web/API/AudioScheduledSourceNode/start) on both oscillators and the LFO. They run continuously; the VCA gate controls whether sound is heard.

---

## Signal Chain

### Oscillators

Two [`OscillatorNode`](https://developer.mozilla.org/en-US/docs/Web/API/OscillatorNode) instances run continuously:

- **`osc`** — Main oscillator. Type is set by the `waveform` parameter (sawtooth or square).
- **`subOsc`** — Sub-oscillator. Always a square wave at half the main oscillator frequency. Mixed through `subGain` (currently set to `0`, meaning it's silent by default but ready to be enabled).

Both feed into the filter node.

### Filter

The filter is the heart of the 303 sound. Two implementations exist, chosen at init time:

**Worklet path** — An [`AudioWorkletNode`](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletNode) running the `DiodeLadderProcessor`. This is a 4-pole (24 dB/oct) resonant lowpass with per-stage `tanh` saturation. The nonlinear saturation is what gives the 303 its characteristic squelch — a linear filter cannot reproduce this. See [worklets/diode-ladder.md](worklets/diode-ladder.md) for implementation details.

**Fallback path** — A [`BiquadFilterNode`](https://developer.mozilla.org/en-US/docs/Web/API/BiquadFilterNode) set to `'lowpass'` type. Functionally similar but without the nonlinear saturation character.

Regardless of which implementation is active, three [`AudioParam`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam) references are exposed:

| Property | Worklet Source | Fallback Source |
|---|---|---|
| `filterFreq` | `filter.parameters.get('frequency')` | `filter.frequency` |
| `filterReso` | `filter.parameters.get('resonance')` | `filter.Q` |
| `filterDetune` | `filter.parameters.get('detune')` | `filter.detune` |

All downstream code (envelope automation, LFO routing, knob changes) uses these three references without knowing which filter is underneath.

### LFO

The LFO is an [`OscillatorNode`](https://developer.mozilla.org/en-US/docs/Web/API/OscillatorNode) connected through a [`GainNode`](https://developer.mozilla.org/en-US/docs/Web/API/GainNode) to `filterDetune`:

```
LFO (OscillatorNode) → lfoGain (GainNode) → filterDetune (AudioParam)
```

This is audio-rate modulation — the LFO output is a signal measured in cents that directly modulates the filter cutoff. Using `detune` (cents) instead of `frequency` (Hz) is important because cents are multiplicative — the modulation depth scales naturally across the frequency range and can never produce negative frequencies.

The LFO supports three waveforms: sine, triangle, and square.

### VCA

A single [`GainNode`](https://developer.mozilla.org/en-US/docs/Web/API/GainNode) (`vca`) acts as the voltage-controlled amplifier. Its gain is `0` when no note is playing and is shaped by the amplitude envelope on each note trigger. This is the gate that controls whether sound passes through the rest of the chain.

### Distortion

Distortion uses a **dual-shaper crossfade** pattern to avoid artifacts:

```
VCA → distA (WaveShaperNode) → distGainA (GainNode) → distWet
VCA → distB (WaveShaperNode) → distGainB (GainNode) → distWet
VCA → distDry (GainNode) → distOut
distWet → distOut
```

Two [`WaveShaperNode`](https://developer.mozilla.org/en-US/docs/Web/API/WaveShaperNode) instances exist, both with `'4x'` oversampling. Only one is active at a time. When the distortion amount changes:

1. A new curve is written to the **inactive** shaper.
2. The inactive shaper's gain is smoothly ramped to `1`.
3. The active shaper's gain is smoothly ramped to `0`.
4. The active/inactive labels swap.

This avoids the audible glitch that occurs when assigning a new `curve` array to a `WaveShaperNode` that is actively processing audio.

The distortion curve is a soft-clipping transfer function:

```
f(x) = ((1 + k) * x) / (1 + k * |x|)
```

where `k = amount * 2`. Higher values of `k` push the curve closer to hard clipping.

### Delay

A tape-style delay built from standard nodes:

```
distOut → delay (DelayNode) → delayFilter (BiquadFilterNode, lowpass 3500 Hz)
                                    ↓                          ↓
                              delayFeedback (GainNode) ──→ delay (feedback loop)
                              delayWet (GainNode) ──→ delayOut
distOut → delayDry (GainNode) ──→ delayOut
```

The [`DelayNode`](https://developer.mozilla.org/en-US/docs/Web/API/DelayNode) feeds back through a lowpass filter, simulating the progressive high-frequency loss of analog tape delay. The [`BiquadFilterNode`](https://developer.mozilla.org/en-US/docs/Web/API/BiquadFilterNode) in the feedback loop ensures each repetition is darker than the last.

### Reverb

Convolution reverb using a [`ConvolverNode`](https://developer.mozilla.org/en-US/docs/Web/API/ConvolverNode) with a procedurally generated impulse response:

```js
_generateReverbIR()
```

This creates a stereo [`AudioBuffer`](https://developer.mozilla.org/en-US/docs/Web/API/AudioBuffer) filled with random noise shaped by a decay envelope: `Math.pow(1 - i / length, 2.5)`. The buffer length is `sampleRate * reverbDecay`, so longer decay values produce longer IR buffers.

**Buffer swap safety** — Assigning a new buffer to a live `ConvolverNode` can cause clicks. The `_scheduleReverbIR()` method debounces rapid changes (e.g., during a knob drag) and wraps the swap in a mute→swap→unmute sequence:

1. Smooth `reverbWet.gain` to `0`.
2. Wait 40 ms for the fade to complete.
3. Generate and assign the new IR buffer.
4. Smooth `reverbWet.gain` back to the target level.

### Master and Limiter

```
reverbOut → master (GainNode, 0.7) → limiter (DynamicsCompressorNode) → analyser → destination
```

The [`DynamicsCompressorNode`](https://developer.mozilla.org/en-US/docs/Web/API/DynamicsCompressorNode) acts as a brick-wall limiter to catch transient spikes (especially from rapid filter coefficient changes or high-resonance sweeps). Settings:

| Parameter | Value | Purpose |
|---|---|---|
| `threshold` | −3 dB | Catches peaks just below clipping |
| `knee` | 0 dB | Hard knee for transparent limiting |
| `ratio` | 20:1 | Near-infinite ratio for brick-wall behavior |
| `attack` | 0.001 s | Near-instant response to transients |
| `release` | 0.05 s | Quick release to avoid pumping |

### Analyser

An [`AnalyserNode`](https://developer.mozilla.org/en-US/docs/Web/API/AnalyserNode) with `fftSize = 2048` sits between the limiter and the destination. The UI's oscilloscope reads time-domain data from it with [`getFloatTimeDomainData()`](https://developer.mozilla.org/en-US/docs/Web/API/AnalyserNode/getFloatTimeDomainData).

When the oscilloscope is disabled, the analyser is **fully disconnected** from the signal chain to save FFT computation on the audio thread.

---

## Note Triggering

```js
synth.triggerNote(midi, time, isAccent, isSlide);
```

| Parameter | Type | Description |
|---|---|---|
| `midi` | `number` | MIDI note number (e.g., `36` = C2) |
| `time` | `number` | [`AudioContext.currentTime`](https://developer.mozilla.org/en-US/docs/Web/API/BaseAudioContext/currentTime) at which to start the note |
| `isAccent` | `boolean` | Whether this is an accented step |
| `isSlide` | `boolean` | Whether to glide from the previous note |

### Frequency Setting

MIDI-to-frequency conversion uses the standard formula:

```js
440 * Math.pow(2, (midi - 69) / 12)
```

The `tuning` parameter (semitones) is added before conversion.

**Slide behavior** — If `isSlide` is `true` and a note is already playing, the oscillator frequencies are ramped with [`linearRampToValueAtTime`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam/linearRampToValueAtTime) over 60 ms, creating the 303's signature portamento. Without slide, frequencies snap instantly via [`setValueAtTime`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam/setValueAtTime).

### Amplitude Envelope

For non-slide notes, the VCA gain follows this shape:

```
0.001 → peakVol (5ms attack) → baseVol*0.4 (decay) → 0.001 (decay+0.1s) → 0 (hard zero)
```

Key techniques:
- **[`exponentialRampToValueAtTime`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam/exponentialRampToValueAtTime)** is used for the decay portions because it produces a constant rate of change in decibels, which sounds natural since loudness perception is logarithmic.
- The final `setValueAtTime(0)` after the exponential ramp to `0.001` is essential because exponential ramps can never reach zero.
- Accented notes have a higher peak volume and shorter decay.

### Filter Envelope

The filter frequency sweeps from a peak cutoff down to the base cutoff:

```
current → peakCutoff (3ms attack) → baseCutoff (decay)
```

The 3 ms attack uses `exponentialRampToValueAtTime` rather than `setValueAtTime` to avoid coefficient discontinuities in the filter. All filter frequency values are clamped to a minimum of 30 Hz to prevent instability.

Accent increases both the envelope modulation depth (1.5× boost) and reduces the decay time (0.7× multiplier), creating the harder, squelchier attack characteristic of accented 303 notes.

---

## Note Release

```js
synth.releaseNote(time);
```

Cancels pending VCA automation and smoothly fades to zero using [`setTargetAtTime`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam/setTargetAtTime) with a 10 ms time constant.

The method prefers [`cancelAndHoldAtTime`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam/cancelAndHoldAtTime) when available. This method freezes the automation value at the specified time without a discontinuity. The fallback (`cancelScheduledValues` + `setValueAtTime(param.value, t)`) reads `.value` at JS execution time, which is incorrect when `t` is in the future.

---

## Parameter Control

```js
synth.setParam(name, value);
```

Updates the internal parameter store and applies the change to the live audio graph. Every continuous parameter change goes through `_smooth()` to ensure click-free updates.

| Parameter Name | Applies To |
|---|---|
| `cutoff` | `filterFreq` |
| `resonance` | `filterReso` |
| `waveform` | `osc.type` (immediate assignment) |
| `distAmount` | Dual-shaper crossfade |
| `distOn` | Distortion dry/wet mix |
| `delayTime` | `delay.delayTime` |
| `delayFeedback` | `delayFeedback.gain` |
| `delayMix` / `delayOn` | Delay dry/wet mix |
| `reverbDecay` | Debounced IR regeneration |
| `reverbMix` / `reverbOn` | Reverb dry/wet mix |
| `lfoRate` | `lfo.frequency` |
| `lfoAmount` | `lfoGain.gain` |
| `lfoWave` | `lfo.type` (immediate assignment) |

---

## Oscilloscope Toggle

```js
synth.disableScope();  // Disconnect analyser, save CPU
synth.enableScope();   // Reconnect analyser
```

When disabled, the limiter connects directly to `ctx.destination`, bypassing the `AnalyserNode` entirely. This saves the FFT computation that the analyser runs on every audio block.

---

## Helper Utilities

### `midiToFreq(n)`

Converts a MIDI note number to frequency in Hz using equal temperament.

### `noteToName(m)`

Converts a MIDI note number to a human-readable string like `C2` or `F#3`.

### `_smooth(param, value, timeConstant)`

The central parameter-smoothing method. Anchors the current value, cancels pending automation, and schedules an exponential approach via [`setTargetAtTime`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam/setTargetAtTime). Default time constant is 15 ms.

### `_buildCurve(arr, amount)`

Fills a `Float32Array` with a soft-clipping transfer function for use as a `WaveShaperNode.curve`.

### `_updateDistortionCurve()`

Implements the dual-shaper crossfade — writes a new curve to the inactive shaper and crossfades.

### `_updateDistMix()` / `_updateDelayMix()` / `_updateReverbMix()`

Smoothly crossfade between dry and wet paths for each effect based on the current on/off state and mix parameter.

### `_scheduleReverbIR()`

Debounces reverb IR regeneration and wraps the buffer swap in a mute→swap→unmute sequence.

### `_generateReverbIR()`

Creates a procedural stereo impulse response buffer with exponential decay.
