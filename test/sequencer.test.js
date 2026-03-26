const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const { loadSequencer } = require('./helpers');

const { Sequencer, AcidSynth } = loadSequencer();

/** Create a minimal mock synth for the Sequencer constructor */
function mockSynth() {
  const s = new AcidSynth();
  // Record calls for assertions
  s._triggered = [];
  s._released = [];
  s.triggerNote = (midi, time, accent, slide) => s._triggered.push({ midi, time, accent, slide });
  s.releaseNote = (time) => s._released.push({ time });
  return s;
}

// ── constructor ───────────────────────────────────────────────
describe('Sequencer constructor', () => {
  it('defaults to 138 bpm', () => {
    const seq = new Sequencer(mockSynth());
    assert.strictEqual(seq.tempo, 138);
  });

  it('starts not playing', () => {
    const seq = new Sequencer(mockSynth());
    assert.strictEqual(seq.isPlaying, false);
  });

  it('initialises currentStep to -1', () => {
    const seq = new Sequencer(mockSynth());
    assert.strictEqual(seq.currentStep, -1);
  });

  it('creates exactly 16 steps', () => {
    const seq = new Sequencer(mockSynth());
    assert.strictEqual(seq.steps.length, 16);
  });

  it('each step has required properties', () => {
    const seq = new Sequencer(mockSynth());
    for (const step of seq.steps) {
      assert.ok('note' in step);
      assert.ok('gate' in step);
      assert.ok('accent' in step);
      assert.ok('slide' in step);
    }
  });
});

// ── tempo ─────────────────────────────────────────────────────
describe('Sequencer.tempo', () => {
  it('getter returns current tempo', () => {
    const seq = new Sequencer(mockSynth());
    assert.strictEqual(seq.tempo, 138);
  });

  it('setter updates internal _tempo', () => {
    const seq = new Sequencer(mockSynth());
    seq.tempo = 120;
    assert.strictEqual(seq.tempo, 120);
  });
});

// ── stepDuration ──────────────────────────────────────────────
describe('Sequencer.stepDuration', () => {
  it('computes correct 16th-note duration at 120 bpm', () => {
    const seq = new Sequencer(mockSynth());
    seq.tempo = 120;
    // 60 / 120 / 4 = 0.125 seconds per 16th note
    assert.ok(Math.abs(seq.stepDuration - 0.125) < 1e-10);
  });

  it('computes correct 16th-note duration at 138 bpm', () => {
    const seq = new Sequencer(mockSynth());
    const expected = 60 / 138 / 4;
    assert.ok(Math.abs(seq.stepDuration - expected) < 1e-10);
  });
});

// ── _initPattern ──────────────────────────────────────────────
describe('Sequencer._initPattern', () => {
  it('sets up the default acid pattern', () => {
    const seq = new Sequencer(mockSynth());
    // Step 0: note 36, gate true, no accent/slide
    assert.strictEqual(seq.steps[0].note, 36);
    assert.strictEqual(seq.steps[0].gate, true);
    assert.strictEqual(seq.steps[0].accent, false);
    assert.strictEqual(seq.steps[0].slide, false);

    // Step 1: note 36, gate true, accent
    assert.strictEqual(seq.steps[1].note, 36);
    assert.strictEqual(seq.steps[1].gate, true);
    assert.strictEqual(seq.steps[1].accent, true);

    // Step 5: gate off
    assert.strictEqual(seq.steps[5].gate, false);
  });
});

// ── clear ─────────────────────────────────────────────────────
describe('Sequencer.clear', () => {
  it('resets all steps to note 36, gate/accent/slide off', () => {
    const seq = new Sequencer(mockSynth());
    seq.clear();
    for (let i = 0; i < 16; i++) {
      assert.strictEqual(seq.steps[i].note, 36);
      assert.strictEqual(seq.steps[i].gate, false);
      assert.strictEqual(seq.steps[i].accent, false);
      assert.strictEqual(seq.steps[i].slide, false);
    }
  });
});

// ── randomize ─────────────────────────────────────────────────
describe('Sequencer.randomize', () => {
  it('produces 16 steps', () => {
    const seq = new Sequencer(mockSynth());
    seq.randomize();
    assert.strictEqual(seq.steps.length, 16);
  });

  it('notes are drawn from minor pentatonic scale + octave', () => {
    const seq = new Sequencer(mockSynth());
    const allowed = new Set([0, 3, 5, 7, 10, 12]);
    // Run many times to reduce flakiness from randomness
    for (let trial = 0; trial < 20; trial++) {
      seq.randomize();
      for (const step of seq.steps) {
        const base = step.note >= 48 ? step.note - 48 : step.note - 36;
        assert.ok(allowed.has(base), `note ${step.note} offset ${base} not in pentatonic scale`);
      }
    }
  });

  it('gate/accent/slide are booleans', () => {
    const seq = new Sequencer(mockSynth());
    seq.randomize();
    for (const step of seq.steps) {
      assert.strictEqual(typeof step.gate, 'boolean');
      assert.strictEqual(typeof step.accent, 'boolean');
      assert.strictEqual(typeof step.slide, 'boolean');
    }
  });
});

// ── _handleStep ───────────────────────────────────────────────
describe('Sequencer._handleStep', () => {
  it('triggers note when gate is on', () => {
    const synth = mockSynth();
    const seq = new Sequencer(synth);
    seq.steps[0] = { note: 48, gate: true, accent: false, slide: false };
    seq.steps[1] = { note: 36, gate: false, accent: false, slide: false };
    seq._handleStep(0, 1.0, 0.125);
    assert.strictEqual(synth._triggered.length, 1);
    assert.strictEqual(synth._triggered[0].midi, 48);
    assert.strictEqual(synth._triggered[0].time, 1.0);
  });

  it('does not trigger note when gate is off', () => {
    const synth = mockSynth();
    const seq = new Sequencer(synth);
    seq.steps[2] = { note: 40, gate: false, accent: false, slide: false };
    seq._handleStep(2, 1.0, 0.125);
    assert.strictEqual(synth._triggered.length, 0);
  });

  it('passes accent flag to triggerNote', () => {
    const synth = mockSynth();
    const seq = new Sequencer(synth);
    seq.steps[3] = { note: 36, gate: true, accent: true, slide: false };
    seq.steps[4] = { note: 36, gate: false, accent: false, slide: false };
    seq._handleStep(3, 1.0, 0.125);
    assert.strictEqual(synth._triggered[0].accent, true);
  });

  it('passes slide flag to triggerNote', () => {
    const synth = mockSynth();
    const seq = new Sequencer(synth);
    seq.steps[5] = { note: 36, gate: true, accent: false, slide: true };
    seq.steps[6] = { note: 36, gate: false, accent: false, slide: false };
    seq._handleStep(5, 1.0, 0.125);
    assert.strictEqual(synth._triggered[0].slide, true);
  });

  it('schedules release when next step has no slide', () => {
    const synth = mockSynth();
    const seq = new Sequencer(synth);
    seq.steps[0] = { note: 48, gate: true, accent: false, slide: false };
    seq.steps[1] = { note: 36, gate: true, accent: false, slide: false };
    seq._handleStep(0, 1.0, 0.2);
    assert.strictEqual(synth._released.length, 1);
    assert.ok(Math.abs(synth._released[0].time - 1.15) < 1e-10);
  });

  it('does not release when next step has gate+slide', () => {
    const synth = mockSynth();
    const seq = new Sequencer(synth);
    seq.steps[0] = { note: 48, gate: true, accent: false, slide: false };
    seq.steps[1] = { note: 50, gate: true, accent: false, slide: true };
    seq._handleStep(0, 1.0, 0.2);
    assert.strictEqual(synth._released.length, 0);
  });

  it('wraps around to step 0 when checking next step of step 15', () => {
    const synth = mockSynth();
    const seq = new Sequencer(synth);
    seq.clear();
    seq.steps[15] = { note: 48, gate: true, accent: false, slide: false };
    seq.steps[0] = { note: 36, gate: true, accent: false, slide: true };
    seq._handleStep(15, 1.0, 0.2);
    // next step (0) has slide + gate, so no release
    assert.strictEqual(synth._released.length, 0);
  });

  it('fires onStep callback', () => {
    const synth = mockSynth();
    const seq = new Sequencer(synth);
    let fired = null;
    seq.onStep = (s) => { fired = s; };
    seq._handleStep(7, 1.0, 0.125);
    assert.strictEqual(fired, 7);
  });

  it('sets currentStep', () => {
    const synth = mockSynth();
    const seq = new Sequencer(synth);
    seq._handleStep(5, 1.0, 0.125);
    assert.strictEqual(seq.currentStep, 5);
  });
});

// ── stop ──────────────────────────────────────────────────────
describe('Sequencer.stop', () => {
  it('sets isPlaying to false and resets currentStep', () => {
    const synth = mockSynth();
    const seq = new Sequencer(synth);
    seq.isPlaying = true;
    seq.currentStep = 5;
    seq.stop();
    assert.strictEqual(seq.isPlaying, false);
    assert.strictEqual(seq.currentStep, -1);
  });

  it('calls releaseNote on synth', () => {
    const synth = mockSynth();
    const seq = new Sequencer(synth);
    seq.stop();
    assert.strictEqual(synth._released.length, 1);
  });

  it('calls onStep(-1)', () => {
    const synth = mockSynth();
    const seq = new Sequencer(synth);
    let stepVal = null;
    seq.onStep = (s) => { stepVal = s; };
    seq.stop();
    assert.strictEqual(stepVal, -1);
  });
});
