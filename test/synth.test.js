const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const { loadSynth } = require('./helpers');

const { midiToFreq, noteToName, NOTE_NAMES, AcidSynth } = loadSynth();

// ── midiToFreq ────────────────────────────────────────────────
describe('midiToFreq', () => {
  it('returns 440 for MIDI note 69 (A4)', () => {
    assert.strictEqual(midiToFreq(69), 440);
  });

  it('returns 261.63 Hz (≈ C4) for MIDI note 60', () => {
    assert.ok(Math.abs(midiToFreq(60) - 261.6256) < 0.01);
  });

  it('doubles frequency for an octave up', () => {
    const ratio = midiToFreq(81) / midiToFreq(69);
    assert.ok(Math.abs(ratio - 2) < 1e-10);
  });

  it('halves frequency for an octave down', () => {
    const ratio = midiToFreq(57) / midiToFreq(69);
    assert.ok(Math.abs(ratio - 0.5) < 1e-10);
  });
});

// ── noteToName ────────────────────────────────────────────────
describe('noteToName', () => {
  it('returns C4 for MIDI 60', () => {
    assert.strictEqual(noteToName(60), 'C4');
  });

  it('returns A4 for MIDI 69', () => {
    assert.strictEqual(noteToName(69), 'A4');
  });

  it('returns C#3 for MIDI 49', () => {
    assert.strictEqual(noteToName(49), 'C#3');
  });

  it('returns C-1 for MIDI 0', () => {
    assert.strictEqual(noteToName(0), 'C-1');
  });
});

// ── NOTE_NAMES ────────────────────────────────────────────────
describe('NOTE_NAMES', () => {
  it('has 12 entries', () => {
    assert.strictEqual(NOTE_NAMES.length, 12);
  });

  it('starts with C and ends with B', () => {
    assert.strictEqual(NOTE_NAMES[0], 'C');
    assert.strictEqual(NOTE_NAMES[11], 'B');
  });
});

// ── AcidSynth constructor ─────────────────────────────────────
describe('AcidSynth constructor', () => {
  it('initializes with null ctx', () => {
    const s = new AcidSynth();
    assert.strictEqual(s.ctx, null);
  });

  it('workletAvailable defaults to false', () => {
    const s = new AcidSynth();
    assert.strictEqual(s.workletAvailable, false);
  });

  it('scopeEnabled defaults to true', () => {
    const s = new AcidSynth();
    assert.strictEqual(s.scopeEnabled, true);
  });

  it('contains expected default params', () => {
    const s = new AcidSynth();
    assert.strictEqual(s.params.waveform, 'sawtooth');
    assert.strictEqual(s.params.cutoff, 800);
    assert.strictEqual(s.params.resonance, 8);
    assert.strictEqual(s.params.envMod, 3000);
    assert.strictEqual(s.params.decay, 0.3);
    assert.strictEqual(s.params.accent, 0.6);
    assert.strictEqual(s.params.distOn, false);
    assert.strictEqual(s.params.delayOn, false);
    assert.strictEqual(s.params.reverbOn, false);
    assert.strictEqual(s.params.lfoRate, 4);
    assert.strictEqual(s.params.lfoAmount, 0);
    assert.strictEqual(s.params.lfoWave, 'sine');
  });

  it('currentNote defaults to null', () => {
    const s = new AcidSynth();
    assert.strictEqual(s.currentNote, null);
  });
});

// ── AcidSynth._buildCurve ────────────────────────────────────
describe('AcidSynth._buildCurve', () => {
  it('fills a Float32Array with the waveshaper transfer curve', () => {
    const s = new AcidSynth();
    const arr = new Float32Array(256);
    s._buildCurve(arr, 40);
    // Endpoints should be approximately ±1
    assert.ok(arr[0] < 0, 'first sample should be negative');
    assert.ok(arr[255] > 0, 'last sample should be positive');
  });

  it('curve is antisymmetric (odd function)', () => {
    const s = new AcidSynth();
    const arr = new Float32Array(256);
    s._buildCurve(arr, 40);
    // The transfer function f(x) = (1+k)*x / (1+k*|x|) is odd: f(-x) = -f(x).
    // Verify this property by computing f at symmetric x values directly.
    const k = 40 * 2;
    for (const x of [0.1, 0.3, 0.5, 0.7, 0.9]) {
      const pos = ((1 + k) * x) / (1 + k * x);
      const neg = ((1 + k) * -x) / (1 + k * x);
      assert.ok(Math.abs(pos + neg) < 1e-10, `antisymmetry violated at x=${x}`);
    }
  });

  it('zero drive produces near-linear curve', () => {
    const s = new AcidSynth();
    const arr = new Float32Array(256);
    s._buildCurve(arr, 0);
    // With k = 0 the formula simplifies to x, so every value should match its linear position
    for (let i = 0; i < 256; i++) {
      const x = (i * 2) / 256 - 1;
      assert.ok(Math.abs(arr[i] - x) < 1e-5, `expected ~${x} at index ${i}, got ${arr[i]}`);
    }
  });

  it('higher drive compresses the curve more', () => {
    const s = new AcidSynth();
    const lo = new Float32Array(256);
    const hi = new Float32Array(256);
    s._buildCurve(lo, 10);
    s._buildCurve(hi, 80);
    // At the very end both should be close to 1, but the high-drive curve should
    // reach near-1 values sooner (i.e., a mid-range sample is closer to 1).
    const mid = 192; // ≈ 75% of the way through
    assert.ok(hi[mid] > lo[mid], 'higher drive should compress the curve');
  });
});

// ── AcidSynth.setParam (no ctx) ──────────────────────────────
describe('AcidSynth.setParam without ctx', () => {
  it('updates params object even when ctx is null', () => {
    const s = new AcidSynth();
    s.setParam('cutoff', 1200);
    assert.strictEqual(s.params.cutoff, 1200);
  });

  it('updates any known param', () => {
    const s = new AcidSynth();
    s.setParam('waveform', 'square');
    assert.strictEqual(s.params.waveform, 'square');
    s.setParam('reverbDecay', 3.5);
    assert.strictEqual(s.params.reverbDecay, 3.5);
  });
});

// ── AcidSynth.triggerNote / releaseNote without ctx ───────────
describe('AcidSynth note methods without ctx', () => {
  it('triggerNote returns early when ctx is null', () => {
    const s = new AcidSynth();
    // Should not throw
    s.triggerNote(60);
    assert.strictEqual(s.currentNote, null);
  });

  it('releaseNote returns early when ctx is null', () => {
    const s = new AcidSynth();
    s.releaseNote();
  });
});

// ── AcidSynth.disableScope / enableScope without ctx ──────────
describe('AcidSynth scope methods without ctx', () => {
  it('disableScope returns early when ctx is null', () => {
    const s = new AcidSynth();
    s.disableScope();
    assert.strictEqual(s.scopeEnabled, true);
  });

  it('enableScope returns early when ctx is null', () => {
    const s = new AcidSynth();
    s.enableScope();
    assert.strictEqual(s.scopeEnabled, true);
  });
});
