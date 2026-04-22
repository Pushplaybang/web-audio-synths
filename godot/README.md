# ACID-303 — Godot Implementation

A TB-303 style monophonic acid synth and step sequencer built in Godot 4.4.

This is a direct port of the [web version](../index.html) using Godot's `AudioStreamGenerator` for per-sample DSP on the audio thread.

## Requirements

- [Godot 4.4+](https://godotengine.org/download) (standard or .NET edition)

## Getting Started

1. Open Godot 4.4
2. Click **Import** and select `godot/project.godot`
3. Press **F5** (or ▶ in the toolbar) to run

## Architecture

### Signal Chain

```
Oscillator (saw/square) → DiodeLadderFilter (4-pole) → Amplitude Envelope
                            ↑ LFO → detune              → Distortion
                                                          → Tape Delay
                                                          → Reverb
                                                          → Soft Clip → Output
```

### File Structure

```
godot/
├── project.godot                 # Godot project config
├── scenes/
│   └── main.tscn                 # Main UI scene
├── scripts/
│   ├── audio/
│   │   ├── synth_spec.gd         # Constants from spec/synth-behavior.json
│   │   ├── acid_synth_engine.gd  # Main synth engine (fills audio buffer)
│   │   ├── oscillator.gd         # Saw/square oscillator
│   │   ├── diode_ladder_filter.gd # 4-pole ladder filter with tanh saturation
│   │   ├── envelope.gd           # Amplitude/filter envelope generator
│   │   ├── lfo.gd                # Low frequency oscillator
│   │   ├── distortion.gd         # Waveshaper distortion
│   │   ├── tape_delay.gd         # Feedback delay with filtered feedback
│   │   └── reverb.gd             # FDN reverb
│   ├── sequencer/
│   │   └── step_sequencer.gd     # 16-step sequencer
│   └── ui/
│       └── main_ui.gd            # UI wiring and keyboard input
├── themes/                        # (future) Godot theme resources
└── assets/                        # (future) icons, fonts
```

### How It Works

1. **`AcidSynthEngine`** extends `Node` and creates an `AudioStreamGenerator` + `AudioStreamPlayer`. Each frame it fills the generator's playback buffer with samples computed from the full DSP chain.

2. **`DiodeLadderFilter`** is a sample-by-sample port of `js/worklets/diode-ladder.js` — same 4-pole ladder with `tanh` saturation per stage, same coefficient math.

3. **`StepSequencer`** counts samples per frame to determine when the next 16th-note step fires. It triggers notes on the synth engine and emits a `step_changed` signal for UI updates.

4. **`main_ui.gd`** connects Godot UI controls (sliders, buttons) to synth engine properties and listens to sequencer signals for visual step highlighting.

### Compared to the Web Version

| Feature | Web | Godot |
|---|---|---|
| Audio DSP | AudioWorklet (diode-ladder.js) | AudioStreamGenerator (per-frame fill) |
| Sequencer clock | AudioWorklet (seq-clock.js) | Sample counter in `_process()` |
| Filter | Identical algorithm | Identical algorithm |
| Oscillator | OscillatorNode (native) | GDScript (naive saw/square) |
| Effects | Web Audio native nodes | GDScript per-sample processing |
| UI | HTML/CSS/DOM | Godot Control nodes |

### Known Limitations

- **GDScript DSP performance**: For complex patches at high sample rates, the per-sample GDScript processing may become CPU-heavy. The path forward is moving DSP to a GDExtension (C++) module.
- **No anti-aliasing**: Oscillators use naive waveforms. Band-limited synthesis (polyBLEP) can be added later.
- **Frame-based sequencer**: Unlike the web version's audio-thread clock, the Godot sequencer advances in `_process()`, which gives frame-level (not sample-level) timing. For tighter timing, move to a dedicated audio thread.
- **Reverb**: Uses a lightweight FDN instead of convolution for performance. Character differs slightly from the web version.

## Keyboard Shortcuts

| Key | Action |
|---|---|
| Space | Play / Stop |
| Z–M | Musical keyboard (C4–C5) |
| ↑ / ↓ | Adjust selected step note ±1 semitone |

## Development

The synth behavior is defined in [`spec/synth-behavior.json`](../spec/synth-behavior.json) — the single source of truth shared across all platform implementations. When adding or changing parameters, update the spec first, then port to each platform.

## Future Work

- [ ] GDExtension (C++) for DSP hotpath
- [ ] Band-limited oscillators (polyBLEP)
- [ ] Audio-thread sequencer clock for tighter timing
- [ ] MIDI input support
- [ ] Preset save/load
- [ ] Custom Godot theme matching the web UI aesthetic
- [ ] Oscilloscope/waveform display
