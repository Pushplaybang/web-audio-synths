// ============================================================
// SEQUENCER
// ============================================================
class Sequencer {
  constructor(synth) {
    this.synth = synth;
    this._tempo = 138;
    this.isPlaying = false;
    this.currentStep = -1;
    this.onStep = null;
    this.clockNode = null;
    this.useWorkletClock = false;
    // Fallback timer state
    this._nextNoteTime = 0;
    this._timerID = null;

    this.steps = Array.from({ length: 16 }, () => ({ note: 36, gate: false, accent: false, slide: false }));
    this._initPattern();
  }

  get tempo() { return this._tempo; }
  set tempo(v) { this._tempo = v; if (this.clockNode) this.clockNode.parameters.get('tempo').value = v; }
  get stepDuration() { return 60 / this._tempo / 4; }

  async init() {
    if (this.clockNode) return;
    if (!this.synth.ctx) return;
    if (this.synth.workletAvailable) {
      this.useWorkletClock = true;
      this.clockNode = new AudioWorkletNode(this.synth.ctx, 'seq-clock', {
        numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [1],
      });
      this.clockNode.parameters.get('tempo').value = this._tempo;
      // Must be connected to graph for process() to fire
      this._sink = this.synth.ctx.createGain();
      this._sink.gain.value = 0;
      this.clockNode.connect(this._sink);
      this._sink.connect(this.synth.ctx.destination);
      // Receive sample-accurate step events from audio thread
      this.clockNode.port.onmessage = (e) => {
        if (e.data.type === 'step') this._handleStep(e.data.step, e.data.time, e.data.stepDuration);
      };
    }
  }

  _handleStep(step, time, dur) {
    this.currentStep = step;
    const s = this.steps[step], ns = this.steps[(step + 1) % 16];
    if (s.gate) {
      this.synth.triggerNote(s.note, time, s.accent, s.slide);
      if (!ns.slide || !ns.gate) this.synth.releaseNote(time + dur * 0.75);
    }
    if (this.onStep) this.onStep(step);
  }

  start() {
    if (this.isPlaying) return;
    this.isPlaying = true;
    if (this.useWorkletClock) {
      this.clockNode.port.postMessage({ type: 'reset' });
      this.clockNode.parameters.get('playing').value = 1;
    } else {
      this.currentStep = -1;
      this._nextNoteTime = this.synth.ctx.currentTime;
      this._scheduleFallback();
    }
  }

  stop() {
    this.isPlaying = false;
    if (this.useWorkletClock) {
      this.clockNode.parameters.get('playing').value = 0;
    } else {
      if (this._timerID) { clearTimeout(this._timerID); this._timerID = null; }
    }
    this.synth.releaseNote();
    this.currentStep = -1;
    if (this.onStep) this.onStep(-1);
  }

  _scheduleFallback() {
    const ahead = 0.1;
    while (this._nextNoteTime < this.synth.ctx.currentTime + ahead) {
      this.currentStep = (this.currentStep + 1) % 16;
      const s = this.steps[this.currentStep], ns = this.steps[(this.currentStep + 1) % 16];
      if (s.gate) {
        this.synth.triggerNote(s.note, this._nextNoteTime, s.accent, s.slide);
        if (!ns.slide || !ns.gate) this.synth.releaseNote(this._nextNoteTime + this.stepDuration * 0.75);
      }
      const idx = this.currentStep;
      const ms = Math.max(0, (this._nextNoteTime - this.synth.ctx.currentTime) * 1000);
      setTimeout(() => { if (this.onStep) this.onStep(idx); }, ms);
      this._nextNoteTime += this.stepDuration;
    }
    this._timerID = setTimeout(() => this._scheduleFallback(), 25);
  }

  _initPattern() {
    const p = [
      { note: 36, gate: true }, { note: 36, gate: true, accent: true },
      { note: 39, gate: true }, { note: 36, gate: true, slide: true },
      { note: 48, gate: true, accent: true }, { note: 36 },
      { note: 39, gate: true }, { note: 41, gate: true, accent: true },
      { note: 36, gate: true }, { note: 36, gate: true, slide: true },
      { note: 48, gate: true }, { note: 46, gate: true, slide: true, accent: true },
      { note: 36, gate: true }, { note: 39 },
      { note: 41, gate: true, accent: true }, { note: 43, gate: true, slide: true },
    ];
    p.forEach((s, i) => { this.steps[i] = { note: s.note||36, gate: s.gate||false, accent: s.accent||false, slide: s.slide||false }; });
  }

  randomize() {
    const sc = [0,3,5,7,10,12];
    for (let i = 0; i < 16; i++) {
      const o = Math.random() < 0.3 ? 48 : 36;
      this.steps[i] = { note: o + sc[Math.floor(Math.random()*sc.length)], gate: Math.random()<0.7, accent: Math.random()<0.25, slide: Math.random()<0.2 };
    }
  }
  clear() { for (let i = 0; i < 16; i++) this.steps[i] = { note: 36, gate: false, accent: false, slide: false }; }
}
