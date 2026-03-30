# sequencer.js — Sequencer Class

`Sequencer` is a 16-step monophonic pattern sequencer. It schedules notes with sample-accurate timing when AudioWorklet is available, and falls back to a `setTimeout`-based lookahead scheduler otherwise.

## Table of Contents

- [Construction](#construction)
- [Step Data Model](#step-data-model)
- [Initialization](#initialization)
- [Timing Strategies](#timing-strategies)
  - [Worklet Clock](#worklet-clock)
  - [Fallback Scheduler](#fallback-scheduler)
- [Transport Controls](#transport-controls)
- [Step Handling](#step-handling)
- [Pattern Management](#pattern-management)

---

## Construction

```js
const seq = new Sequencer(synth);
```

| Parameter | Type | Description |
|---|---|---|
| `synth` | `AcidSynth` | Reference to the audio engine, used to trigger and release notes |

The constructor initializes:

- Default tempo of **138 BPM**
- A 16-element `steps` array with a built-in acid pattern
- Transport state (`isPlaying`, `currentStep`)
- An `onStep` callback slot for UI updates

No [`AudioContext`](https://developer.mozilla.org/en-US/docs/Web/API/AudioContext) interaction occurs at construction time.

---

## Step Data Model

Each of the 16 steps is a plain object:

```js
{
  note:   36,     // MIDI note number (24–72)
  gate:   false,  // Whether the step produces sound
  accent: false,  // Whether the step is accented (louder, squelchier)
  slide:  false   // Whether to glide from the previous note
}
```

- **Gate** — When `false`, the step is silent (a rest). When `true`, the synth triggers a note.
- **Accent** — Increases peak volume, adds extra filter envelope modulation, and shortens decay. This is the core of the 303's rhythmic character.
- **Slide** — When the current step and the next step both have `gate: true` and the current step has `slide: true`, the oscillator frequency glides smoothly rather than jumping. The VCA envelope is not re-triggered, sustaining the previous note's tail into the new pitch.

---

## Initialization

```js
await seq.init();
```

Like `AcidSynth.init()`, this is async and idempotent. It checks whether the synth has a live `AudioContext` and whether AudioWorklet is available:

- **If worklet is available** — Creates an [`AudioWorkletNode`](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletNode) running the `seq-clock` processor. The node must be connected to the audio graph for its `process()` method to fire, so it is connected through a silent [`GainNode`](https://developer.mozilla.org/en-US/docs/Web/API/GainNode) (gain = 0) to `ctx.destination`.
- **If worklet is unavailable** — No initialization needed; the fallback uses `setTimeout` and [`AudioContext.currentTime`](https://developer.mozilla.org/en-US/docs/Web/API/BaseAudioContext/currentTime) directly.

---

## Timing Strategies

### Worklet Clock

When AudioWorklet is available, timing runs entirely on the audio thread via the `SeqClockProcessor` (see [worklets/seq-clock.md](worklets/seq-clock.md)).

**How it works:**

1. The processor counts samples per 16th note at the current tempo.
2. At each step boundary, it posts a message through the node's [`MessagePort`](https://developer.mozilla.org/en-US/docs/Web/API/MessagePort):
   ```js
   { type: 'step', step: 0–15, time: <audioContextTime>, stepDuration: <seconds> }
   ```
3. The main thread receives this message and calls `_handleStep()`.

The `time` field is derived from `currentTime` on the audio thread, giving **sample-accurate scheduling**. The main thread's event loop latency does not affect note placement because all [`AudioParam`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam) automation is scheduled at the precise audio-thread time, not at the moment the message arrives.

**Transport controls via AudioParam:**

| AudioParam | Value | Effect |
|---|---|---|
| `playing` | `1` | Clock runs, step events are posted |
| `playing` | `0` | Clock stops, step counter resets |
| `tempo` | `40–300` | BPM, updates immediately |

A `reset` message posted via `port.postMessage` resets the sample counter and step index to their initial state.

### Fallback Scheduler

When AudioWorklet is not available, the sequencer uses the "lookahead scheduler" pattern:

```js
_scheduleFallback()
```

This is a `setTimeout` loop that fires every **25 ms** and schedules notes up to **100 ms** ahead:

1. Compare `_nextNoteTime` against `AudioContext.currentTime + 0.1` (the lookahead window).
2. For each step that falls within the window, call `synth.triggerNote()` / `synth.releaseNote()` with the precise `_nextNoteTime` value.
3. Schedule UI callbacks using `setTimeout` with a calculated delay.
4. Advance `_nextNoteTime` by `stepDuration` (= `60 / tempo / 4` seconds per 16th note).
5. Re-schedule the loop with `setTimeout(fn, 25)`.

The key insight is that `setTimeout` is only used to **wake up** the scheduler frequently. Actual note scheduling uses [`AudioContext.currentTime`](https://developer.mozilla.org/en-US/docs/Web/API/BaseAudioContext/currentTime) for the timing reference, which is much more precise than `setTimeout` alone.

---

## Transport Controls

### `start()`

Begins playback:

- **Worklet path** — Posts a `reset` message and sets the `playing` AudioParam to `1`.
- **Fallback path** — Resets `currentStep` to `-1`, captures the current `AudioContext.currentTime` as the first note time, and starts the lookahead loop.

### `stop()`

Stops playback:

- **Worklet path** — Sets the `playing` AudioParam to `0`. The processor resets internally.
- **Fallback path** — Clears the `setTimeout` timer.
- Both paths call `synth.releaseNote()` to silence any ringing note and fire `onStep(-1)` to clear the UI highlight.

### `tempo` (getter/setter)

```js
seq.tempo = 160;       // Set BPM
console.log(seq.tempo); // Read BPM
```

The setter updates the internal `_tempo` value and, if the worklet clock is active, writes the new value to the `tempo` AudioParam immediately. The fallback scheduler picks up the new tempo on its next loop iteration because `stepDuration` is recomputed from `_tempo` on each access.

### `stepDuration` (getter)

```js
const dur = seq.stepDuration; // Seconds per 16th note
```

Returns `60 / tempo / 4` — the duration of one step in seconds at the current tempo.

---

## Step Handling

### `_handleStep(step, time, dur)`

Called on every step boundary by either the worklet message handler or the fallback scheduler.

| Parameter | Type | Description |
|---|---|---|
| `step` | `number` | Current step index (0–15) |
| `time` | `number` | Audio-thread time for this step |
| `dur` | `number` | Step duration in seconds |

Logic:

1. Update `currentStep`.
2. If the step's `gate` is `true`:
   - Call `synth.triggerNote()` with the step's note, accent, and slide flags.
   - If the **next** step does not have `slide: true` (or has `gate: false`), schedule `synth.releaseNote()` at 75% of the step duration. This creates the characteristic short, staccato 303 gate.
3. Fire the `onStep` callback so the UI can highlight the active step.

The 75% gate length means notes are held for three-quarters of the step, leaving a brief silence before the next step — unless slide is active, in which case the note sustains into the next step.

---

## Pattern Management

### `_initPattern()`

Loads a hard-coded acid bass pattern. This is the default pattern heard when the user first clicks Play.

### `randomize()`

Generates a new random pattern using a minor pentatonic scale plus octave:

```js
const scale = [0, 3, 5, 7, 10, 12]; // Semitones from root
```

Each step gets:
- A random note from the scale in either the low octave (C2, 70% chance) or high octave (C3, 30% chance)
- A gate with 70% probability
- An accent with 25% probability
- A slide with 20% probability

This produces musically interesting patterns because the scale choice avoids dissonant intervals, and the probability distribution favors more notes on than off.

### `clear()`

Resets all 16 steps to `{ note: 36, gate: false, accent: false, slide: false }` — all gates off at C2.
