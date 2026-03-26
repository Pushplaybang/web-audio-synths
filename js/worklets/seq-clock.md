# seq-clock.js — SeqClockProcessor

`SeqClockProcessor` is an [`AudioWorkletProcessor`](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletProcessor) that acts as a sample-accurate sequencer clock. It counts samples on the audio thread and posts step-boundary events to the main thread via [`MessagePort`](https://developer.mozilla.org/en-US/docs/Web/API/MessagePort).

## Table of Contents

- [Why a Worklet Clock?](#why-a-worklet-clock)
- [AudioParam Interface](#audioparam-interface)
- [Algorithm](#algorithm)
  - [Sample Counting](#sample-counting)
  - [Step Detection](#step-detection)
  - [Message Format](#message-format)
- [Transport Control](#transport-control)
- [Internal State](#internal-state)
- [Registration](#registration)

---

## Why a Worklet Clock?

JavaScript's `setTimeout` and `setInterval` are imprecise — they run on the main thread and are subject to event loop delays, garbage collection pauses, and tab throttling. A `setTimeout(fn, 25)` call might actually fire 30–100 ms late if the main thread is busy.

The audio thread, by contrast, processes exactly 128 samples every `128 / sampleRate` seconds (≈2.67 ms at 48 kHz). It is the most precise, jitter-free timer available in the browser.

By counting samples on the audio thread, the clock can determine **exactly** when each step boundary occurs relative to [`currentTime`](https://developer.mozilla.org/en-US/docs/Web/API/BaseAudioContext/currentTime) — the same timeline used by [`AudioParam`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam) automation. This means note scheduling commands (e.g., `setValueAtTime`, `exponentialRampToValueAtTime`) are placed at the precise sample where the step begins, regardless of main-thread latency.

---

## AudioParam Interface

Declared in [`parameterDescriptors`](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletProcessor/parameterDescriptors):

| Parameter | Default | Min | Max | Rate | Description |
|---|---|---|---|---|---|
| `tempo` | 138 | 40 | 300 | k-rate | Tempo in beats per minute |
| `playing` | 0 | 0 | 1 | k-rate | Transport state: 0 = stopped, 1 = playing |

Both parameters are [k-rate](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam#k-rate), meaning they are read once per 128-sample block. This is appropriate because:

- **Tempo** changes are infrequent and do not need per-sample granularity.
- **Playing** is a binary state toggle.

---

## Algorithm

### Sample Counting

The processor maintains a running sample counter (`sc`) that increments by 1 for each sample in the 128-sample block:

```js
for (let i = 0; i < 128; i++) {
  // ... step detection logic ...
  this.sc++;
}
```

The number of samples per 16th note (one step) is:

```js
const sps = (60 / tempo / 4) * sampleRate;
```

For example, at 138 BPM and 48 kHz sample rate:
```
sps = (60 / 138 / 4) * 48000 = 0.1087 * 48000 ≈ 5217.4 samples per step
```

### Step Detection

The current step index is derived from the sample counter:

```js
const step = Math.floor(this.sc / sps) % 16;
```

When `step` differs from the previously recorded step (`cs`), a step boundary has occurred. The processor posts a message and updates `cs`.

This approach is simple and drift-free — the step number is always derived from the total sample count, not from accumulating durations. Even across millions of samples, the step boundaries stay perfectly aligned with the audio timeline.

### Message Format

On each step boundary, the processor posts a message via its [`MessagePort`](https://developer.mozilla.org/en-US/docs/Web/API/MessagePort):

```js
this.port.postMessage({
  type: 'step',
  step: step,               // 0–15
  time: currentTime + i / sampleRate,  // Precise audio-thread time
  stepDuration: sps / sampleRate       // Duration of one step in seconds
});
```

| Field | Type | Description |
|---|---|---|
| `type` | `string` | Always `'step'` for step events |
| `step` | `number` | Step index in the 16-step pattern (0–15) |
| `time` | `number` | [`currentTime`](https://developer.mozilla.org/en-US/docs/Web/API/BaseAudioContext/currentTime) at the exact sample where this step starts |
| `stepDuration` | `number` | Duration of this step in seconds |

The `time` value is derived from the audio-thread's `currentTime` global plus the offset of the specific sample within the 128-sample block (`i / sampleRate`). This gives sub-block timing precision — the step boundary is located to the **individual sample**, not just to the block boundary.

The main thread uses this `time` value directly in [`AudioParam`](https://developer.mozilla.org/en-US/docs/Web/API/AudioParam) scheduling calls, ensuring note onsets are sample-accurate even though the message arrives on the main thread with some latency.

---

## Transport Control

### Playing State

The `playing` AudioParam controls whether the clock is running:

- **`playing >= 0.5`** — The clock counts samples and posts step events.
- **`playing < 0.5`** — The clock stops. If it was previously playing, it resets the sample counter and step index, and posts a `{ type: 'stop' }` message.

### Reset Message

The main thread can send a reset command via the port:

```js
clockNode.port.postMessage({ type: 'reset' });
```

This resets the sample counter (`sc`) to 0 and the current step (`cs`) to -1. The sequencer sends this before starting playback to ensure the pattern begins from step 0.

---

## Internal State

| Property | Type | Description |
|---|---|---|
| `sc` | `number` | Running sample counter since last reset |
| `cs` | `number` | Current step index (-1 when stopped) |

The state is minimal — just two numbers. All timing is derived mathematically from the sample counter, so there is no accumulated error over time.

### Silent Output

The processor's output buffer is filled with zeros:

```js
const out = outputs[0] && outputs[0][0];
if (out) out.fill(0);
```

The `SeqClockProcessor` does not produce audio. However, it must be connected to the audio graph (via a silent [`GainNode`](https://developer.mozilla.org/en-US/docs/Web/API/GainNode)) for the Web Audio engine to call its `process()` method. Unconnected nodes are not processed — this is a specification requirement.

---

## Registration

```js
registerProcessor('seq-clock', SeqClockProcessor);
```

This makes the processor available to the main thread via:

```js
new AudioWorkletNode(ctx, 'seq-clock', { ... });
```

See the MDN documentation on [`registerProcessor`](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletGlobalScope/registerProcessor) for details.
