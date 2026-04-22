# ACID-303

A TB-303 style monophonic acid synth and step sequencer — multi-platform.

## Platforms

| Platform | Status | Path |
|---|---|---|
| **Web** (reference implementation) | ✅ Stable | [`index.html`](./index.html) |
| **Godot 4.4** (game/desktop) | 🚧 In progress | [`godot/`](./godot/) |
| **Desktop** (Electron/Tauri wrap) | 📋 Planned | — |
| **AU/VST** (DAW plugin via JUCE) | 📋 Planned | — |

## Features

- Monophonic acid-style voice (saw/square oscillator)
- 4-pole diode ladder filter with per-stage tanh saturation
- 16-step sequencer with gate, accent, and slide
- Filter & amplitude envelopes with accent modulation
- LFO → filter detune modulation
- Effects: distortion, tape delay, reverb
- AudioWorklet DSP with automatic fallback (web)

## Shared Spec

All platform implementations are driven by a single [synth behavior spec](./spec/synth-behavior.json) that defines parameter ranges, defaults, envelope shapes, sequencer rules, and the filter algorithm. When porting to a new platform, implement against this spec and use the web version as the reference.

## Web Version

No build step or external dependencies required.

```bash
node server.js
# or
npm start
```

Then open [http://localhost:3000](http://localhost:3000) in a modern browser and click **Play** to start the sequencer.

Set a custom port with `PORT=8080 node server.js` (on Windows: `set PORT=8080 && node server.js`).

## Godot Version

See [`godot/README.md`](./godot/README.md) for setup and architecture.

## Tests

```bash
npm test
```

Runs the Node.js test suite covering synth math, sequencer logic, and AudioWorklet processors.

## License

See [LICENSE](./LICENSE).
