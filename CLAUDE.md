# ACID-303 — Browser-Based TB-303 Emulator

## Project Overview

A monophonic acid synth emulator built with the Web Audio API and AudioWorklet. No frameworks, no build tools, no dependencies — vanilla JS, HTML, and CSS in a single file. Uses AudioWorklet for a custom diode ladder filter and sample-accurate sequencer clock, with automatic fallback to built-in nodes when AudioWorklet is unavailable.

## Architecture

### Signal Chain

```
Oscillator (saw/square) ──┐
                          ├──→ Diode Ladder Filter (AudioWorklet) → VCA (GainNode)
Sub Oscillator (square) ──┘         ↑                                → Distortion (dual WaveShaperNode crossfade)
                              LFO (OscillatorNode                    → Tape Delay (DelayNode + filtered feedback)
                               → GainNode                            → Reverb (ConvolverNode)
                               → filter.detune)                      → Limiter (DynamicsCompressorNode)
                                                                     → Analyser → Output
Sequencer Clock (AudioWorklet) ──→ [MessagePort] ──→ Main thread note scheduling
```

### AudioWorklet Processors

Both processors are defined as inline strings and loaded at runtime via Blob URLs. This keeps the single-file architecture while running DSP on the audio thread.

**`DiodeLadderProcessor`** — A 4-pole (24dB/oct) resonant lowpass filter with per-stage `tanh` saturation modeling the TB-303's transistor ladder. Uses `Float64Array` for numerical stability. AudioParams: `frequency` (a-rate), `resonance` (a-rate), `detune` (a-rate, cents). The nonlinear saturation per stage is what gives the 303 its characteristic squelch — a linear `BiquadFilterNode` can't reproduce this. Also eliminates the coefficient instability that causes crackling with `BiquadFilterNode` at high Q/low cutoff.

**`SeqClockProcessor`** — Sample counter running on the audio thread. Counts samples per 16th note at the current tempo, posts `{ step, time, stepDuration }` messages to the main thread at each step boundary. The `time` field is derived from `currentTime` on the audio thread, giving sample-accurate scheduling with zero main-thread jitter. AudioParams: `tempo` (k-rate), `playing` (k-rate).

### Fallback Architecture

If AudioWorklet Blob loading fails (some CSP policies, older browsers), the engine falls back to:
- `BiquadFilterNode` (lowpass) instead of the diode ladder
- `setTimeout`-based lookahead scheduler instead of the worklet clock

The fallback is transparent: `filterFreq`, `filterReso`, and `filterDetune` are AudioParam references regardless of implementation. A badge in the header shows `WORKLET` or `FALLBACK` to indicate which engine is active.

### Core Classes

- **`AcidSynth`** — Audio engine. Owns AudioContext and node graph. Exposes unified `filterFreq`/`filterReso`/`filterDetune` AudioParam references that point to either the worklet or BiquadFilter params. Handles envelopes, effects, and all parameter smoothing.
- **`Sequencer`** — 16-step sequencer. Uses worklet clock when available, setTimeout fallback otherwise. Step data lives on main thread for UI access. Worklet posts timing events, main thread handles note scheduling.
- **`UI`** — DOM interaction. Drag knobs, step grid, waveform toggles, keyboard input, oscilloscope with enable/disable.

### CSS GPU Compositing

Elements that animate frequently are promoted to their own GPU compositing layers:
- `will-change: transform` on `.knob-cap` (rotated by knob interaction)
- `will-change: contents` on scope `canvas` (redrawn every frame at 60fps)
- `will-change: background-color, box-shadow` on `.step-btn` (sequencer highlight)
- `will-change: transform` on `.toggle-switch::after` (slide animation)
- `contain: layout style paint` on `.section`, `.step-btn`, `.scope-container` (limits browser relayout/repaint scope)

### Key Web Audio Patterns

- **AudioWorklet for custom DSP**: Diode ladder filter running per-sample math on the audio thread
- **AudioWorklet for timing**: Sequencer clock with sample-accurate step boundaries
- **Blob URL module loading**: Inline worklet code loaded without separate files or a build system
- **Audio-rate modulation**: LFO → GainNode → `filter.detune` AudioParam (works identically for both worklet and BiquadFilter)
- **Smooth parameter changes**: `_smooth()` anchors current value then uses `setTargetAtTime` for click-free exponential approach
- **Dual-node crossfade**: Distortion uses two WaveShaperNodes — writes new curve to silent one, crossfades
- **Debounced buffer swap**: Reverb IR regeneration is debounced and wrapped in mute→swap→unmute
- **Exponential envelope ramps**: Filter frequency attack uses `exponentialRampToValueAtTime` for uniform log-space coefficient changes
- **Hard-zero VCA tail**: `exponentialRamp(0.001)` followed by `setValueAtTime(0)` to kill oscillator bleed
- **Brick-wall limiter**: DynamicsCompressorNode catches transient spikes from filter coefficient changes
- **Scope lifecycle**: AnalyserNode fully disconnected from signal chain when scope is off (saves FFT computation on audio thread), rAF loop exits via guard clause (saves main thread)
- **Pre-allocated scope buffer**: `Float32Array` reused across frames to avoid GC pressure

## Development Conventions

### File Structure

Currently single-file (`index.html`). If splitting for the tutorial series:

```
acid-303/
├── CLAUDE.md
├── index.html               # Full implementation
├── style.css                 # Extracted styles
├── js/
│   ├── synth.js              # AcidSynth class
│   ├── sequencer.js          # Sequencer class
│   ├── ui.js                 # UI class
│   └── worklets/
│       ├── diode-ladder.js   # Filter worklet processor
│       └── seq-clock.js      # Sequencer clock processor
└── tutorial/
    ├── 01-drone/             # Osc → Filter → Amp → Output
    ├── 02-envelopes/         # Filter + amp envelopes
    ├── 03-keyboard/          # Note input, monophonic voice
    ├── 04-accent-slide/      # 303-specific features
    ├── 05-sequencer/         # Step sequencer with clock-accurate timing
    ├── 06-effects/           # Distortion, delay, reverb
    ├── 07-lfo/               # LFO modulation via audio-rate connection
    ├── 08-worklet-filter/    # Custom diode ladder filter AudioWorklet
    ├── 09-worklet-clock/     # Sample-accurate sequencer clock
    └── 10-polish/            # Limiter, GPU compositing, click-free techniques
```

### Code Style

- ES6 classes, no modules (runs without a server via `file://`)
- No build tools, bundlers, or transpilers
- Private-by-convention methods prefixed with `_`
- All AudioParam changes go through `_smooth()` — never assign `.value` directly on a live node
- Worklet code is inline strings loaded via Blob URL
- Comments explain *why*, not *what*
- CSS uses custom properties for theming, `will-change` and `contain` for GPU compositing

### Critical Audio Rules

1. **Never set `AudioParam.value` directly while audio is playing.** Use `_smooth()`.
2. **Never use `setValueAtTime` for filter frequency envelope attacks.** Use `exponentialRampToValueAtTime` with 2-3ms attack.
3. **Never hot-swap `WaveShaperNode.curve` on a live signal.** Use dual-shaper crossfade.
4. **Never assign `ConvolverNode.buffer` without muting wet signal.** Debounce + mute→swap→unmute.
5. **Never ramp filter frequency below 30Hz.** Clamp all targets and anchors.
6. **Always anchor before cancelling.** `cancelScheduledValues` → `setValueAtTime(param.value, now)` → new automation.
7. **Use `filter.detune` for LFO modulation, not `filter.frequency`.** Cents are multiplicative, can't produce negative frequencies.
8. **Always follow `exponentialRamp(0.001)` with `setValueAtTime(0)`.** Exponential can't reach zero.
9. **Use AudioContext.currentTime for scheduling.** Never use setTimeout timing for note placement.
10. **AudioWorklet with fallback.** Always provide a BiquadFilter/setTimeout fallback path for environments where worklet Blob loading fails.

## Testing

Manual testing checklist:

- [ ] Badge shows `WORKLET` in supported browsers
- [ ] Play/stop — no lingering hum after stop
- [ ] Adjust every knob while playing — no crackling
- [ ] Toggle distortion/delay/reverb on/off while playing — no clicks
- [ ] High resonance (>20) with low cutoff (<200Hz) — no crackling
- [ ] Rapid tempo changes while playing — timing stays tight
- [ ] Slide between notes — smooth portamento
- [ ] Accent notes — audibly louder with more filter sweep
- [ ] LFO at max amount — no crackling or instability
- [ ] Scope toggle on/off — no audio glitch, canvas stops updating
- [ ] Reverb decay knob rapid adjustment — no clicks
- [ ] Falls back gracefully when AudioWorklet unavailable

## Useful References

- [Web Audio API Spec](https://webaudio.github.io/web-audio-api/)
- [AudioWorklet](https://webaudio.github.io/web-audio-api/#AudioWorklet) — processor lifecycle, AudioParam semantics
- [AudioParam automation](https://webaudio.github.io/web-audio-api/#AudioParam) — setValueAtTime, ramps, setTargetAtTime
- [Tale of Two Clocks](https://web.dev/articles/audio-scheduling) — the lookahead scheduler pattern (used in fallback)
- [Enter AudioWorklet](https://developer.chrome.com/blog/audio-worklet/) — Chrome team's AudioWorklet guide
