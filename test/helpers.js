// Shared helpers for loading browser JS files inside Node's VM sandbox.
const { readFileSync } = require('node:fs');
const { join } = require('node:path');
const vm = require('node:vm');

const ROOT = join(__dirname, '..');

/**
 * Load synth.js into a sandbox and return its exports.
 * Mocks just enough of the browser / Web Audio API for unit-level testing.
 *
 * ES6 class/const/function declarations are block-scoped inside vm contexts
 * and don't become properties of the sandbox object.  We extract them with
 * a follow-up expression that still has access to the script scope.
 */
function loadSynth() {
  const code = readFileSync(join(ROOT, 'js', 'synth.js'), 'utf8');
  const sandbox = { Math, console, setTimeout, clearTimeout, Float32Array };
  vm.createContext(sandbox);
  vm.runInContext(code, sandbox);
  return vm.runInContext('({ midiToFreq, noteToName, NOTE_NAMES, AcidSynth })', sandbox);
}

/**
 * Load sequencer.js (which depends on synth.js globals) and return exports.
 */
function loadSequencer() {
  const synthCode = readFileSync(join(ROOT, 'js', 'synth.js'), 'utf8');
  const seqCode = readFileSync(join(ROOT, 'js', 'sequencer.js'), 'utf8');
  const sandbox = { Math, console, setTimeout, clearTimeout, Float32Array, Array };
  vm.createContext(sandbox);
  vm.runInContext(synthCode, sandbox);
  vm.runInContext(seqCode, sandbox);
  return vm.runInContext('({ Sequencer, AcidSynth, midiToFreq, noteToName })', sandbox);
}

/**
 * Load an AudioWorklet processor file.
 * Provides the minimal AudioWorkletProcessor base class and globals.
 */
function loadWorklet(filename) {
  const code = readFileSync(join(ROOT, 'js', 'worklets', filename), 'utf8');
  const registered = {};
  const sandbox = {
    Math,
    Float64Array,
    Float32Array,
    console,
    sampleRate: 44100,
    currentTime: 0,
    AudioWorkletProcessor: class AudioWorkletProcessor {
      constructor() {
        this.port = { onmessage: null, postMessage() {} };
      }
    },
    registerProcessor(name, ctor) {
      registered[name] = ctor;
    },
  };
  vm.createContext(sandbox);
  vm.runInContext(code, sandbox);
  // registerProcessor is called at module level, so registered is already populated.
  return registered;
}

module.exports = { loadSynth, loadSequencer, loadWorklet };
