# ACID-303 — Browser-Based TB-303 Emulator

## Project Overview

A monophonic acid synth emulator built entirely with the Web Audio API. No frameworks, no build tools, no dependencies — vanilla JS, HTML, and CSS in a single file. The project is a learning vehicle for Web Audio API concepts through the lens of a classic hardware synth.

## Architecture

### Signal Chain

```
Oscillator (saw/square) ──┐
                          ├──→ BiquadFilter (lowpass) → VCA (GainNode) → Distortion (dual WaveShaperNode crossfade)
Sub Oscillator (square) ──┘         ↑                                     → Tape Delay (DelayNode + feedback)
                              LFO (OscillatorNode                         → Reverb (ConvolverNode)
                               → GainNode                                 → Limiter (DynamicsCompressorNode)
                               → filter.detune)                           → Master → Analyser → Output
```

### Core Classes

- **`AcidSynth`** — Audio engine. Owns the AudioContext and entire node graph. Handles note triggering with 303-style filter/amp envelopes, slide (portamento), and accent. Exposes `setParam()` for all real-time parameter changes.
- **`Sequencer`** — 16-step sequencer with per-step gate, accent, slide, and note. Uses AudioContext clock (`currentTime`) for scheduling — never `setTimeout`/`setInterval` for musical timing. The lookahead scheduler pattern runs a JS timer that schedules notes slightly ahead of when they're needed.
- **`UI`** — All DOM interaction. Drag-based knobs, step grid, waveform toggles, keyboard input, oscilloscope canvas.

### Key Web Audio Patterns Used

- **Audio-rate modulation**: LFO → GainNode → `filter.detune` (OscillatorNode output connected to an AudioParam, modulates at sample rate)
- **Smooth parameter changes**: All real-time knob changes go through `_smooth()` which anchors current value with `setValueAtTime` then uses `setTargetAtTime` for click-free exponential approach
- **Dual-node crossfade**: Distortion uses two WaveShaperNodes — writes new curve to silent one, crossfades gains — because `WaveShaperNode.curve` assignment is atomic and causes clicks
- **Debounced buffer swap**: Reverb IR regeneration is debounced and wrapped in a mute→swap→unmute sequence because ConvolverNode buffer replacement clicks
- **Exponential envelope ramps for filter**: Filter frequency is logarithmic, so envelope attack uses `exponentialRampToValueAtTime` to distribute coefficient changes evenly in log-space, preventing BiquadFilterNode instability at high Q
- **Hard-zero VCA tail**: `exponentialRampToValueAtTime` can't target 0 (mathematically undefined), so we ramp to 0.001 then schedule `setValueAtTime(0)` 1ms later to kill oscillator bleed
- **Brick-wall limiter**: DynamicsCompressorNode before output catches transient spikes from high-Q filter coefficient changes

## Development Conventions

### File Structure

Currently single-file (`index.html`). If splitting for the tutorial series:

```
acid-303/
├── CLAUDE.md
├── index.html          # Full implementation
├── style.css           # Extracted styles
├── js/
│   ├── synth.js        # AcidSynth class
│   ├── sequencer.js    # Sequencer class
│   └── ui.js           # UI class
└── tutorial/           # Phased builds
    ├── 01-drone/       # Osc → Filter → Amp → Output
    ├── 02-envelopes/   # Filter + amp envelopes
    ├── 03-keyboard/    # Note input, monophonic voice
    ├── 04-accent-slide/# 303-specific features
    ├── 05-sequencer/   # Step sequencer with clock-accurate timing
    ├── 06-effects/     # Distortion, delay, reverb
    ├── 07-lfo/         # LFO modulation via audio-rate connection
    └── 08-polish/      # Limiter, click-free techniques, UI
```

### Code Style

- ES6 classes, no modules (runs without a server via `file://`)
- No build tools, bundlers, or transpilers
- Private-by-convention methods prefixed with `_`
- All AudioParam changes go through `_smooth()` — never assign `.value` directly on a live node
- Comments explain *why*, not *what* — focus on Web Audio gotchas and the reasoning behind each pattern
- CSS uses custom properties (`--var`) for theming, no preprocessors

### Critical Audio Rules

These are hard-won lessons — do not regress on any of them:

1. **Never set `AudioParam.value` directly while audio is playing.** Use `_smooth()` (setValueAtTime + setTargetAtTime) or scheduled ramps.
2. **Never use `setValueAtTime` for filter frequency envelope attacks.** Use `exponentialRampToValueAtTime` with a 2-3ms attack time.
3. **Never hot-swap `WaveShaperNode.curve` on a live signal.** Use the dual-shaper crossfade pattern.
4. **Never assign `ConvolverNode.buffer` without muting the wet signal first.** Debounce + mute→swap→unmute.
5. **Never ramp filter frequency to values below 30Hz.** Clamp all filter frequency targets and anchors to ≥30.
6. **Always anchor before cancelling.** After `cancelScheduledValues`, call `setValueAtTime(param.value, now)` before starting a new automation curve.
7. **Use `filter.detune` for LFO modulation, not `filter.frequency`.** Detune (cents) is multiplicative and can never produce negative frequencies.
8. **Always follow `exponentialRampToValueAtTime(0.001, t)` with `setValueAtTime(0, t + epsilon)`.** The exponential function never reaches zero — schedule a hard zero after the ramp to prevent oscillator bleed.
9. **Use AudioContext.currentTime for musical scheduling.** The lookahead scheduler pattern: a `setTimeout` loop checks if notes need scheduling, and schedules them with precise `currentTime` offsets. Never use `setTimeout` timing directly for note placement.

### UI Conventions

- Knobs: drag up/down, 200px = full range. Exponential curve option for frequency params.
- All displayed values include units (Hz, ms, ct, dB, s)
- Fonts: Orbitron (displays/labels), JetBrains Mono (body)
- Color scheme: dark panel aesthetic with orange (#ff5722) accent

## Testing

No automated tests currently. Manual testing checklist:

- [ ] Play/stop sequencer — no lingering hum after stop
- [ ] Adjust every knob while sequencer is playing — no crackling
- [ ] Toggle distortion/delay/reverb on/off while playing — no clicks
- [ ] High resonance (>20) with low cutoff (<200Hz) — no crackling
- [ ] Rapid tempo changes while playing — timing stays tight
- [ ] Slide between notes — smooth portamento, no retrigger
- [ ] Accent notes — audibly louder with more filter sweep
- [ ] LFO at max amount with low cutoff — no crackling or instability
- [ ] Keyboard input while sequencer is running — no conflicts
- [ ] Reverb decay knob rapid adjustment — no clicks

## Useful References

- [Web Audio API Spec](https://webaudio.github.io/web-audio-api/) — the authoritative source
- [AudioParam automation](https://webaudio.github.io/web-audio-api/#AudioParam) — setValueAtTime, linearRamp, exponentialRamp, setTargetAtTime semantics
- [Tale of Two Clocks](https://web.dev/articles/audio-scheduling) — the lookahead scheduler pattern this sequencer uses
- [BiquadFilterNode](https://webaudio.github.io/web-audio-api/#BiquadFilterNode) — coefficient computation, stability considerations
