# ui.js — UI Class

`UI` wires all DOM interaction to the `AcidSynth` and `Sequencer` classes. It handles knob dragging, toggle switches, waveform selection, transport buttons, the step sequencer grid, keyboard input, and the oscilloscope display.

## Table of Contents

- [Construction](#construction)
- [Knob Interaction](#knob-interaction)
- [Toggle Switches](#toggle-switches)
- [Waveform Selection](#waveform-selection)
- [Transport Controls](#transport-controls)
- [Step Sequencer Grid](#step-sequencer-grid)
- [Keyboard Input](#keyboard-input)
- [Oscilloscope](#oscilloscope)

---

## Construction

```js
const ui = new UI(synth, seq);
```

The constructor calls all `_init*` methods immediately, binding event listeners to the existing DOM elements rendered by `index.html`. No dynamic DOM creation happens at construction time — only event binding.

All initialization methods that react to user input call `await synth.init()` (and `await seq.init()` where needed) before doing anything. This ensures the [`AudioContext`](https://developer.mozilla.org/en-US/docs/Web/API/AudioContext) is created on the first user gesture, satisfying browser autoplay policies.

---

## Knob Interaction

### How Knobs Work

Each knob is a `.knob-container` element with `data-*` attributes that define its parameter binding and range:

| Attribute | Example | Description |
|---|---|---|
| `data-param` | `"cutoff"` | Parameter name passed to `synth.setParam()` |
| `data-min` | `"60"` | Minimum value |
| `data-max` | `"8000"` | Maximum value |
| `data-value` | `"800"` | Current / initial value |
| `data-curve` | `"exp"` | Mapping curve (`"linear"` or `"exp"`) |
| `data-step` | `"1"` | Optional quantization step |

### Interaction Model

Knobs respond to **vertical drag** — dragging up increases the value, dragging down decreases it. The drag distance maps through a normalized `0–1` range to the output value.

**Linear mapping:**
```
value = min + (max - min) * normalized
```

**Exponential mapping** (used for frequency parameters like cutoff):
```
value = min * Math.pow(max / min, normalized)
```

Exponential mapping is essential for frequency controls because human pitch perception is logarithmic. A linear knob would spend most of its travel in the high-frequency range, making low frequencies hard to dial in. The exponential curve gives equal perceptual resolution across the whole range.

### Visual Feedback

Each knob has two visual components updated on every drag frame:

- **`.knob-cap`** — Rotated from −135° to +135° (270° total sweep).
- **`.knob-ring`** — A CSS `conic-gradient` that fills a colored arc proportional to the current value.

### Touch Support

All knob interactions support both mouse and touch events. Touch handlers use `{ passive: false }` to allow `preventDefault()` and prevent the page from scrolling during knob drags.

### Double-Click Reset

Double-clicking a knob resets it to its `data-value` (the initial value defined in HTML).

---

## Toggle Switches

Toggle switches (`.toggle-switch` elements) control boolean parameters like `distOn`, `delayOn`, and `reverbOn`.

Clicking a toggle:
1. Initializes the audio engine (first-gesture safety).
2. Toggles the `.active` CSS class.
3. Calls `synth.setParam()` with the `data-toggle` attribute name and the new boolean state.

---

## Waveform Selection

Two groups of waveform buttons exist:

- **Oscillator waveforms** (`.wave-btn:not(.lfo-wave)`) — Toggle between `sawtooth` and `square` for the main oscillator.
- **LFO waveforms** (`.lfo-wave`) — Toggle between `sine`, `triangle`, and `square` for the LFO shape.

Both groups are mutually exclusive — clicking one button deactivates the others in its group and calls `synth.setParam()` with the selected waveform.

---

## Transport Controls

| Button | Action |
|---|---|
| **Play** | Initializes audio + sequencer, calls `seq.start()` |
| **Stop** | Calls `seq.stop()`, clears step highlight |
| **Random** | Calls `seq.randomize()`, re-renders the step grid |
| **Tempo ▲ / ▼** | Adjusts tempo ±2 BPM (clamped to 40–300), updates display |

The sequencer's `onStep` callback is set to `_hlStep()`, which toggles the `.current` CSS class on step buttons to show the playback position.

---

## Step Sequencer Grid

### Structure

The sequencer UI has three rows:

1. **Step numbers** (`#stepNumbers`) — Static labels 1–16, created once during initialization.
2. **Step buttons** (`#stepGrid`) — Interactive buttons for toggling gate/accent/slide.
3. **Note display** (`#noteDisplay`) — Shows the note name for each step, clickable for step selection.

### Edit Modes

A row of mode buttons controls what clicking a step button toggles:

| Mode | Effect |
|---|---|
| `gate` | Toggle whether the step plays a note |
| `accent` | Toggle accent on the step |
| `slide` | Toggle slide (portamento) on the step |
| `clear` | Reset all steps to off |

The active mode determines the CSS class applied to active steps: `.gate-on`, `.accent-on`, or `.slide-on`.

### Note Editing

Clicking a note cell in the display row selects that step for editing (highlighted with `.editing` class). With a step selected:

- **Arrow Up / Arrow Down** — Adjusts the note ±1 semitone (range: MIDI 24–72, i.e., C1–C5).
- **Keyboard note keys** — Sets the step's note and gate, then advances to the next step. This allows rapid pattern entry by playing notes on the keyboard.

### Rendering

`_renderSeq()` rebuilds the step grid and note display from scratch on every change. This is a simple and reliable approach — the 32 DOM elements (16 buttons + 16 note cells) are cheap to recreate.

---

## Keyboard Input

The keyboard maps a single octave of keys to MIDI notes:

| Key | Note | MIDI |
|---|---|---|
| Z | C3 | 48 |
| S | C#3 | 49 |
| X | D3 | 50 |
| D | D#3 | 51 |
| C | E3 | 52 |
| V | F3 | 53 |
| G | F#3 | 54 |
| B | G3 | 55 |
| H | G#3 | 56 |
| N | A3 | 57 |
| J | A#3 | 58 |
| M | B3 | 59 |
| , | C4 | 60 |

This follows the standard "piano keyboard on QWERTY" layout where the bottom row is white keys and the home row fills in sharps/flats.

**Key down** calls `synth.triggerNote()`. **Key up** calls `synth.releaseNote()`. Key repeat events are ignored to prevent re-triggering on held keys.

**Spacebar** toggles play/stop, initializing the audio engine on first press.

---

## Oscilloscope

### Implementation

The oscilloscope renders waveform data from the synth's [`AnalyserNode`](https://developer.mozilla.org/en-US/docs/Web/API/AnalyserNode) onto a `<canvas>` element using [`requestAnimationFrame`](https://developer.mozilla.org/en-US/docs/Web/API/Window/requestAnimationFrame).

Each frame:

1. Resizes the canvas to match its CSS size × `devicePixelRatio` for sharp rendering on high-DPI displays.
2. Draws faint horizontal grid lines.
3. Reads time-domain data via [`getFloatTimeDomainData()`](https://developer.mozilla.org/en-US/docs/Web/API/AnalyserNode/getFloatTimeDomainData) into a pre-allocated `Float32Array` (reused across frames to avoid garbage collection pressure).
4. Draws the waveform as a continuous path with a glow effect (`shadowBlur`).

### Performance

- The `Float32Array` buffer is allocated once and reused — no allocation per frame.
- The `requestAnimationFrame` loop uses a guard clause (`if (!this.scopeActive) return`) to fully exit when the scope is disabled, rather than continuing to fire and returning early.
- When disabled, `synth.disableScope()` disconnects the `AnalyserNode` from the signal chain, eliminating FFT computation on the audio thread.

### Toggle

The scope toggle button (`.scope-toggle`) controls both the visual rendering and the audio-thread analyser connection:

- **Enable** — Reconnects the analyser node, starts the `requestAnimationFrame` loop.
- **Disable** — Disconnects the analyser, clears the canvas, and lets the `requestAnimationFrame` loop exit.
