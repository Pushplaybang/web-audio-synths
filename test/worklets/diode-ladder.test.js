const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const { loadWorklet } = require('../helpers');

const processors = loadWorklet('diode-ladder.js');
const DiodeLadderProcessor = processors['diode-ladder'];

// ── registration ──────────────────────────────────────────────
describe('DiodeLadderProcessor registration', () => {
  it('registers under the name "diode-ladder"', () => {
    assert.ok(DiodeLadderProcessor, 'processor should be registered');
  });
});

// ── parameterDescriptors ──────────────────────────────────────
describe('DiodeLadderProcessor.parameterDescriptors', () => {
  const descriptors = DiodeLadderProcessor.parameterDescriptors;

  it('exposes three parameters', () => {
    assert.strictEqual(descriptors.length, 3);
  });

  it('has frequency param with default 800', () => {
    const f = descriptors.find(d => d.name === 'frequency');
    assert.ok(f);
    assert.strictEqual(f.defaultValue, 800);
    assert.strictEqual(f.minValue, 20);
    assert.strictEqual(f.maxValue, 18000);
    assert.strictEqual(f.automationRate, 'a-rate');
  });

  it('has resonance param with default 8', () => {
    const r = descriptors.find(d => d.name === 'resonance');
    assert.ok(r);
    assert.strictEqual(r.defaultValue, 8);
    assert.strictEqual(r.minValue, 0);
    assert.strictEqual(r.maxValue, 30);
  });

  it('has detune param with default 0', () => {
    const d = descriptors.find(d => d.name === 'detune');
    assert.ok(d);
    assert.strictEqual(d.defaultValue, 0);
    assert.strictEqual(d.minValue, -4800);
    assert.strictEqual(d.maxValue, 4800);
  });
});

// ── tanh approximation (t) ────────────────────────────────────
describe('DiodeLadderProcessor.t (tanh approximation)', () => {
  let proc;
  it('can be instantiated', () => {
    proc = new DiodeLadderProcessor();
    assert.ok(proc);
  });

  it('returns 0 for input 0', () => {
    assert.strictEqual(proc.t(0), 0);
  });

  it('clamps positive values above 3 to 1', () => {
    assert.strictEqual(proc.t(3.5), 1);
    assert.strictEqual(proc.t(100), 1);
  });

  it('clamps negative values below -3 to -1', () => {
    assert.strictEqual(proc.t(-3.5), -1);
    assert.strictEqual(proc.t(-100), -1);
  });

  it('is an odd function: t(-x) === -t(x)', () => {
    for (const x of [0.1, 0.5, 1.0, 2.0, 2.9]) {
      assert.ok(Math.abs(proc.t(-x) + proc.t(x)) < 1e-12, `odd-symmetry violated at x=${x}`);
    }
  });

  it('approximates real tanh within the active range', () => {
    for (const x of [0.1, 0.5, 1.0, 2.0]) {
      const approx = proc.t(x);
      const real = Math.tanh(x);
      assert.ok(Math.abs(approx - real) < 0.15, `t(${x})=${approx} too far from tanh(${x})=${real}`);
    }
  });
});

// ── process ───────────────────────────────────────────────────
describe('DiodeLadderProcessor.process', () => {
  it('returns true (keep-alive)', () => {
    const proc = new DiodeLadderProcessor();
    const inp = new Float32Array(128).fill(0);
    const out = new Float32Array(128);
    const result = proc.process([[inp]], [[out]], {
      frequency: new Float32Array([800]),
      resonance: new Float32Array([8]),
      detune: new Float32Array([0]),
    });
    assert.strictEqual(result, true);
  });

  it('outputs silence for silent input', () => {
    const proc = new DiodeLadderProcessor();
    const inp = new Float32Array(128).fill(0);
    const out = new Float32Array(128);
    proc.process([[inp]], [[out]], {
      frequency: new Float32Array([800]),
      resonance: new Float32Array([8]),
      detune: new Float32Array([0]),
    });
    for (let i = 0; i < 128; i++) {
      assert.strictEqual(out[i], 0, `expected silence at sample ${i}`);
    }
  });

  it('produces non-zero output for non-zero input', () => {
    const proc = new DiodeLadderProcessor();
    const inp = new Float32Array(128);
    for (let i = 0; i < 128; i++) inp[i] = Math.sin(2 * Math.PI * 440 * i / 44100);
    const out = new Float32Array(128);
    proc.process([[inp]], [[out]], {
      frequency: new Float32Array([8000]),
      resonance: new Float32Array([0]),
      detune: new Float32Array([0]),
    });
    const maxAbs = out.reduce((m, v) => Math.max(m, Math.abs(v)), 0);
    assert.ok(maxAbs > 0.001, `expected non-zero output, got max abs ${maxAbs}`);
  });

  it('attenuates high-frequency content with low cutoff', () => {
    // Generate a high-frequency sine well above the cutoff
    const proc1 = new DiodeLadderProcessor();
    const proc2 = new DiodeLadderProcessor();
    const N = 1024;
    const inp = new Float32Array(N);
    for (let i = 0; i < N; i++) inp[i] = Math.sin(2 * Math.PI * 5000 * i / 44100);

    const outHigh = new Float32Array(N);
    const outLow = new Float32Array(N);

    // High cutoff — signal should pass through
    for (let off = 0; off < N; off += 128) {
      const chunk = inp.subarray(off, off + 128);
      const outChunk = outHigh.subarray(off, off + 128);
      proc1.process([[chunk]], [[outChunk]], {
        frequency: new Float32Array([12000]),
        resonance: new Float32Array([0]),
        detune: new Float32Array([0]),
      });
    }
    // Low cutoff — signal should be attenuated
    for (let off = 0; off < N; off += 128) {
      const chunk = inp.subarray(off, off + 128);
      const outChunk = outLow.subarray(off, off + 128);
      proc2.process([[chunk]], [[outChunk]], {
        frequency: new Float32Array([200]),
        resonance: new Float32Array([0]),
        detune: new Float32Array([0]),
      });
    }

    const rmsHigh = Math.sqrt(outHigh.reduce((s, v) => s + v * v, 0) / N);
    const rmsLow = Math.sqrt(outLow.reduce((s, v) => s + v * v, 0) / N);
    assert.ok(rmsHigh > rmsLow * 2, `high-cutoff RMS ${rmsHigh} should be much larger than low-cutoff RMS ${rmsLow}`);
  });

  it('handles missing input gracefully', () => {
    const proc = new DiodeLadderProcessor();
    const out = new Float32Array(128);
    const result = proc.process([[]], [[out]], {
      frequency: new Float32Array([800]),
      resonance: new Float32Array([8]),
      detune: new Float32Array([0]),
    });
    assert.strictEqual(result, true);
  });

  it('supports a-rate (per-sample) parameter arrays', () => {
    const proc = new DiodeLadderProcessor();
    const inp = new Float32Array(128);
    for (let i = 0; i < 128; i++) inp[i] = Math.sin(2 * Math.PI * 440 * i / 44100);
    const out = new Float32Array(128);
    // Provide per-sample frequency array
    const freqArr = new Float32Array(128).fill(2000);
    const resoArr = new Float32Array(128).fill(10);
    const detArr = new Float32Array(128).fill(0);
    const result = proc.process([[inp]], [[out]], {
      frequency: freqArr,
      resonance: resoArr,
      detune: detArr,
    });
    assert.strictEqual(result, true);
    const maxAbs = out.reduce((m, v) => Math.max(m, Math.abs(v)), 0);
    assert.ok(maxAbs > 0, 'should produce output with per-sample params');
  });
});
