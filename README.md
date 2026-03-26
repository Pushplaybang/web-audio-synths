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

No build step or external dependencies are required.

```bash
node server.js
# or
npm start
```

Then open [http://localhost:3000](http://localhost:3000) in a modern browser and click **Play** to start the sequencer.

Set a custom port with `PORT=8080 node server.js`.

## License

See [LICENSE](./LICENSE).
