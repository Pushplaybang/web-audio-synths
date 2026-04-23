# ACID-303 — JUCE VST/AU Plugin

A TB-303 style monophonic acid synth and step sequencer as a VST3/AU/Standalone plugin, built with [JUCE 8](https://juce.com/).

This is a direct port of the [web version](../index.html) using the [shared synth behavior spec](../spec/synth-behavior.json), optimized for DAW-quality audio with band-limited oscillators and oversampled distortion.

## Requirements

- [CMake 3.22+](https://cmake.org/download/)
- C and C++17 compiler (Clang, GCC, or MSVC — both C and C++ compilers are required by JUCE)
- macOS: Xcode 14+ (for AU builds)
- Windows: Visual Studio 2019+ (for VST3 builds)
- Linux: ALSA dev headers (`libasound2-dev`), plus X11/GL headers for GUI

JUCE 8 is fetched automatically via CMake's FetchContent — no manual download needed.

## Building

```bash
cd juce
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Build outputs

| Format | Location |
|---|---|
| **VST3** | `build/Acid303_artefacts/Release/VST3/ACID-303.vst3` |
| **AU** (macOS only) | `build/Acid303_artefacts/Release/AU/ACID-303.component` |
| **Standalone** | `build/Acid303_artefacts/Release/Standalone/ACID-303` |

### Installing

Copy the built plugin to your DAW's plugin folder:

- **macOS VST3**: `~/Library/Audio/Plug-Ins/VST3/`
- **macOS AU**: `~/Library/Audio/Plug-Ins/Components/`
- **Windows VST3**: `C:\Program Files\Common Files\VST3\`
- **Linux VST3**: `~/.vst3/`

Then rescan plugins in your DAW.

## Architecture

### Signal Chain

```
Oscillator (polyBLEP saw/square) → Diode Ladder Filter (4-pole, double precision)
                                      ↑ LFO → detune (cents)
                                      ↑ Filter envelope
                                   → VCA (amplitude envelope)
                                   → Distortion (4x oversampled waveshaper)
                                   → Tape Delay (feedback + lowpass)
                                   → Reverb (4-line FDN)
                                   → Master volume + soft clip → Output
```

### File Structure

```
juce/
├── CMakeLists.txt            # Build config — fetches JUCE 8 automatically
├── README.md
└── Source/
    ├── SynthSpec.h            # Constants from spec/synth-behavior.json
    ├── DiodeLadderFilter.h    # 4-pole ladder, double precision, tanh saturation
    ├── Oscillator.h           # PolyBLEP band-limited saw/square
    ├── Envelope.h             # Amplitude/filter envelopes with accent
    ├── LFO.h                  # Sine/triangle/square → filter detune
    ├── Distortion.h           # 4x oversampled waveshaper
    ├── TapeDelay.h            # Feedback delay with lowpass in feedback path
    ├── Reverb.h               # 4-line FDN algorithmic reverb
    ├── Sequencer.h            # 16-step sequencer (sample-accurate on audio thread)
    ├── AcidSynthEngine.h      # Full signal chain combining all DSP
    ├── PluginProcessor.h/.cpp # JUCE AudioProcessor + APVTS parameter wiring
    └── PluginEditor.h/.cpp    # GUI: knobs, step grid, transport, waveform selectors
```

### Quality Improvements Over Web/Godot

| Feature | Web/Godot | JUCE |
|---|---|---|
| Oscillator | Naive saw/square | **PolyBLEP** band-limited (no aliasing) |
| Distortion | Direct waveshape | **4x oversampled** waveshaper |
| Filter precision | Float64 / GDScript float | **double** precision (same algorithm) |
| Sequencer timing | Frame-based / worklet | **Sample-accurate** on audio thread |
| Parameter smoothing | setTargetAtTime / manual | **APVTS** + per-block sync |
| State save/restore | N/A | **Full preset** save/restore via XML |
| MIDI input | Computer keyboard only | **Native MIDI** from DAW/controller |

### How It Works

1. **`PluginProcessor`** owns the `AcidSynthEngine` and an `AudioProcessorValueTreeState` with all 20+ parameters. Each `processBlock()` call syncs APVTS values to the engine, processes MIDI, and renders audio sample-by-sample.

2. **`AcidSynthEngine`** wires together all DSP components in a single `processSample()` method — identical signal flow to the web version.

3. **`Sequencer`** runs on the audio thread via `processSample()` called once per sample. Sample-counter timing gives jitter-free step boundaries matching the web AudioWorklet clock.

4. **`PluginEditor`** provides the full GUI with rotary knobs (APVTS-attached), waveform selector buttons, toggle switches for effects, step grid with gate/accent/slide modes, and transport controls.

## Parameters

All parameters are automatable in any DAW:

| Parameter | Range | Default | Unit |
|---|---|---|---|
| Waveform | Sawtooth / Square | Sawtooth | — |
| Tuning | -12 – 12 | 0 | semitones |
| Cutoff | 60 – 8000 | 800 | Hz |
| Resonance | 0 – 30 | 8 | — |
| Env Mod | 1 – 8000 | 3000 | Hz |
| Decay | 0.02 – 1.5 | 0.3 | s |
| Accent | 0 – 1 | 0.6 | — |
| Distortion On | off/on | off | — |
| Drive | 0 – 100 | 40 | — |
| Delay On | off/on | off | — |
| Delay Time | 0.05 – 1.0 | 0.375 | s |
| Delay Feedback | 0 – 0.9 | 0.45 | — |
| Delay Mix | 0 – 1 | 0.3 | — |
| Reverb On | off/on | off | — |
| Reverb Decay | 0.3 – 5.0 | 2.0 | s |
| Reverb Mix | 0 – 1 | 0.2 | — |
| LFO Rate | 0.05 – 30 | 4 | Hz |
| LFO Amount | 0 – 4800 | 0 | cents |
| LFO Wave | Sine / Tri / Square | Sine | — |
| Tempo | 40 – 300 | 138 | BPM |

## Development

The synth behavior is defined in [`spec/synth-behavior.json`](../spec/synth-behavior.json). When adding or changing parameters, update the spec first, then port to each platform.

## Future Work

- [ ] SIMD-optimized DSP inner loops
- [ ] Proper 2x/4x oversampling on the entire signal chain (not just distortion)
- [ ] Custom look-and-feel matching the web CSS theme exactly
- [ ] Preset management with factory patches
- [ ] Host transport sync for the sequencer
- [ ] MPE / per-note expression support
