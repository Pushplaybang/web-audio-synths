const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const { loadWorklet } = require('../helpers');

const processors = loadWorklet('seq-clock.js');
const SeqClockProcessor = processors['seq-clock'];

// ── registration ──────────────────────────────────────────────
describe('SeqClockProcessor registration', () => {
  it('registers under the name "seq-clock"', () => {
    assert.ok(SeqClockProcessor, 'processor should be registered');
  });
});

// ── parameterDescriptors ──────────────────────────────────────
describe('SeqClockProcessor.parameterDescriptors', () => {
  const descriptors = SeqClockProcessor.parameterDescriptors;

  it('exposes two parameters', () => {
    assert.strictEqual(descriptors.length, 2);
  });

  it('has tempo param with default 138', () => {
    const t = descriptors.find(d => d.name === 'tempo');
    assert.ok(t);
    assert.strictEqual(t.defaultValue, 138);
    assert.strictEqual(t.minValue, 40);
    assert.strictEqual(t.maxValue, 300);
    assert.strictEqual(t.automationRate, 'k-rate');
  });

  it('has playing param with default 0', () => {
    const p = descriptors.find(d => d.name === 'playing');
    assert.ok(p);
    assert.strictEqual(p.defaultValue, 0);
    assert.strictEqual(p.minValue, 0);
    assert.strictEqual(p.maxValue, 1);
    assert.strictEqual(p.automationRate, 'k-rate');
  });
});

// ── process (stopped) ─────────────────────────────────────────
describe('SeqClockProcessor.process (stopped)', () => {
  it('returns true when not playing', () => {
    const proc = new SeqClockProcessor();
    const out = new Float32Array(128);
    const result = proc.process([], [[out]], {
      tempo: new Float32Array([138]),
      playing: new Float32Array([0]),
    });
    assert.strictEqual(result, true);
  });

  it('fills output with zeros when not playing', () => {
    const proc = new SeqClockProcessor();
    const out = new Float32Array(128).fill(1);
    proc.process([], [[out]], {
      tempo: new Float32Array([138]),
      playing: new Float32Array([0]),
    });
    for (let i = 0; i < 128; i++) {
      assert.strictEqual(out[i], 0, `expected 0 at sample ${i}`);
    }
  });
});

// ── process (playing) ─────────────────────────────────────────
describe('SeqClockProcessor.process (playing)', () => {
  it('returns true when playing', () => {
    const proc = new SeqClockProcessor();
    const out = new Float32Array(128);
    const result = proc.process([], [[out]], {
      tempo: new Float32Array([138]),
      playing: new Float32Array([1]),
    });
    assert.strictEqual(result, true);
  });

  it('posts a step message on the first render quantum', () => {
    const messages = [];
    const proc = new SeqClockProcessor();
    proc.port.postMessage = (msg) => messages.push(msg);
    const out = new Float32Array(128);
    proc.process([], [[out]], {
      tempo: new Float32Array([138]),
      playing: new Float32Array([1]),
    });
    assert.ok(messages.length >= 1, 'should post at least one step message');
    assert.strictEqual(messages[0].type, 'step');
    assert.strictEqual(messages[0].step, 0);
  });

  it('advances through steps over many render quanta', () => {
    const messages = [];
    const proc = new SeqClockProcessor();
    proc.port.postMessage = (msg) => messages.push(msg);
    const out = new Float32Array(128);

    // Process enough quanta to advance past step 0
    // At 138bpm, 16th note = 44100 * 60 / 138 / 4 ≈ 4793 samples
    // Each quantum = 128 samples, so ~38 quanta per step
    for (let i = 0; i < 200; i++) {
      proc.process([], [[out]], {
        tempo: new Float32Array([138]),
        playing: new Float32Array([1]),
      });
    }
    // Should have seen multiple distinct steps
    const stepsSeen = new Set(messages.filter(m => m.type === 'step').map(m => m.step));
    assert.ok(stepsSeen.size > 1, `expected multiple steps, got ${[...stepsSeen]}`);
  });

  it('step messages include time and stepDuration', () => {
    const messages = [];
    const proc = new SeqClockProcessor();
    proc.port.postMessage = (msg) => messages.push(msg);
    const out = new Float32Array(128);
    proc.process([], [[out]], {
      tempo: new Float32Array([138]),
      playing: new Float32Array([1]),
    });
    const stepMsg = messages.find(m => m.type === 'step');
    assert.ok(stepMsg);
    assert.strictEqual(typeof stepMsg.time, 'number');
    assert.strictEqual(typeof stepMsg.stepDuration, 'number');
    assert.ok(stepMsg.stepDuration > 0);
  });

  it('wraps step count around 16', () => {
    const messages = [];
    const proc = new SeqClockProcessor();
    proc.port.postMessage = (msg) => messages.push(msg);
    const out = new Float32Array(128);

    // Run enough quanta for a full cycle (16 steps * ~38 quanta ≈ 608+)
    for (let i = 0; i < 700; i++) {
      proc.process([], [[out]], {
        tempo: new Float32Array([138]),
        playing: new Float32Array([1]),
      });
    }
    const steps = messages.filter(m => m.type === 'step').map(m => m.step);
    // All steps should be in [0, 15]
    for (const s of steps) {
      assert.ok(s >= 0 && s <= 15, `step ${s} out of range`);
    }
    // Should have wrapped back to step 0 at least twice
    const zeroCount = steps.filter(s => s === 0).length;
    assert.ok(zeroCount >= 2, `expected step 0 at least twice, got ${zeroCount}`);
  });
});

// ── reset ─────────────────────────────────────────────────────
describe('SeqClockProcessor reset', () => {
  it('resets sample counter and step on "reset" message', () => {
    const messages = [];
    const proc = new SeqClockProcessor();
    proc.port.postMessage = (msg) => messages.push(msg);
    const out = new Float32Array(128);

    // Advance a bit
    for (let i = 0; i < 80; i++) {
      proc.process([], [[out]], {
        tempo: new Float32Array([138]),
        playing: new Float32Array([1]),
      });
    }
    const before = messages.filter(m => m.type === 'step').length;

    // Reset
    proc.port.onmessage({ data: { type: 'reset' } });

    // Process again — should restart from step 0
    messages.length = 0;
    proc.process([], [[out]], {
      tempo: new Float32Array([138]),
      playing: new Float32Array([1]),
    });
    const stepMsg = messages.find(m => m.type === 'step');
    assert.ok(stepMsg);
    assert.strictEqual(stepMsg.step, 0);
  });
});

// ── stop message ──────────────────────────────────────────────
describe('SeqClockProcessor stop message', () => {
  it('posts stop message when playing transitions to stopped', () => {
    const messages = [];
    const proc = new SeqClockProcessor();
    proc.port.postMessage = (msg) => messages.push(msg);
    const out = new Float32Array(128);

    // Start playing
    proc.process([], [[out]], {
      tempo: new Float32Array([138]),
      playing: new Float32Array([1]),
    });

    messages.length = 0;
    // Stop
    proc.process([], [[out]], {
      tempo: new Float32Array([138]),
      playing: new Float32Array([0]),
    });
    const stopMsg = messages.find(m => m.type === 'stop');
    assert.ok(stopMsg, 'expected a stop message');
  });
});
