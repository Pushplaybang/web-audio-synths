# ACID-303

A browser-based TB-303 style synth and step sequencer built with the Web Audio API.

## What this is

ACID-303 is a vanilla JavaScript synthesizer app with:

- Monophonic acid-style voice (saw/square)
- Resonant lowpass filtering with modulation
- 16-step sequencer with gate, accent, and slide
- Built-in effects (distortion, delay, reverb)
- AudioWorklet-powered processing with fallback behavior

## Run locally

No build step or dependencies are required.

1. Open `/home/runner/work/web-audio-synths/web-audio-synths/index.html` in a modern browser.
2. Click **Play** to start the sequencer.

## Project structure

- `index.html` — app layout and bootstrapping
- `style.css` — synth UI styling
- `js/synth.js` — audio engine
- `js/sequencer.js` — step sequencer logic
- `js/ui.js` — UI behavior
- `js/worklets/` — AudioWorklet processors

## License

See [LICENSE](./LICENSE).
