// ============================================================
// ACID-303 — TB-303 Emulator with AudioWorklet
// ============================================================
const NOTE_NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
function midiToFreq(n) { return 440 * Math.pow(2, (n - 69) / 12); }
function noteToName(m) { return NOTE_NAMES[m % 12] + Math.floor(m / 12 - 1); }

// ============================================================
// SYNTH ENGINE
// ============================================================
class AcidSynth {
  constructor() {
    this.ctx = null;
    this.workletAvailable = false;
    this.scopeEnabled = true;
    this.params = {
      waveform: 'sawtooth', tuning: 0, cutoff: 800, resonance: 8,
      envMod: 3000, decay: 0.3, accent: 0.6,
      distOn: false, distAmount: 40,
      delayOn: false, delayTime: 0.375, delayFeedback: 0.45, delayMix: 0.3,
      reverbOn: false, reverbDecay: 2.0, reverbMix: 0.2,
      lfoRate: 4, lfoAmount: 0, lfoWave: 'sine',
    };
    this.currentNote = null;
  }

  async init() {
    if (this.ctx) return;
    this.ctx = new AudioContext();

    // Try loading AudioWorklet processors from separate files.
    // Resolve paths relative to the document so GitHub Pages deployment works.
    try {
      const base = new URL('.', document.baseURI).href;
      await Promise.all([
        this.ctx.audioWorklet.addModule(new URL('js/worklets/diode-ladder.js', base).href),
        this.ctx.audioWorklet.addModule(new URL('js/worklets/seq-clock.js', base).href),
      ]);
      this.workletAvailable = true;
    } catch (e) {
      console.warn('AudioWorklet unavailable, using fallback:', e);
      this.workletAvailable = false;
    }

    const badge = document.getElementById('engineBadge');
    badge.textContent = this.workletAvailable ? 'WORKLET' : 'FALLBACK';
    badge.style.color = this.workletAvailable ? 'var(--green)' : 'var(--yellow)';
    badge.style.borderColor = this.workletAvailable ? 'rgba(76,175,80,0.3)' : 'rgba(255,193,7,0.3)';

    // === OSC ===
    this.osc = this.ctx.createOscillator();
    this.osc.type = this.params.waveform;
    this.osc.frequency.value = 220;
    this.subOsc = this.ctx.createOscillator();
    this.subOsc.type = 'square';
    this.subOsc.frequency.value = 110;
    this.subGain = this.ctx.createGain();
    this.subGain.gain.value = 0;

    // === FILTER ===
    // Diode ladder worklet or BiquadFilter fallback — either way,
    // filterFreq/filterReso/filterDetune are AudioParam objects
    // with identical automation APIs. All downstream code is the same.
    if (this.workletAvailable) {
      this.filter = new AudioWorkletNode(this.ctx, 'diode-ladder', {
        numberOfInputs: 1, numberOfOutputs: 1, channelCount: 1,
      });
      this.filterFreq = this.filter.parameters.get('frequency');
      this.filterReso = this.filter.parameters.get('resonance');
      this.filterDetune = this.filter.parameters.get('detune');
    } else {
      this.filter = this.ctx.createBiquadFilter();
      this.filter.type = 'lowpass';
      this.filterFreq = this.filter.frequency;
      this.filterReso = this.filter.Q;
      this.filterDetune = this.filter.detune;
    }
    this.filterFreq.value = this.params.cutoff;
    this.filterReso.value = this.params.resonance;

    // === LFO ===
    this.lfo = this.ctx.createOscillator();
    this.lfo.type = this.params.lfoWave;
    this.lfo.frequency.value = this.params.lfoRate;
    this.lfoGain = this.ctx.createGain();
    this.lfoGain.gain.value = this.params.lfoAmount;
    this.lfo.connect(this.lfoGain);
    this.lfoGain.connect(this.filterDetune);
    this.lfo.start();

    // === VCA ===
    this.vca = this.ctx.createGain();
    this.vca.gain.value = 0;

    // === DISTORTION (dual crossfade) ===
    this.distA = this.ctx.createWaveShaper(); this.distB = this.ctx.createWaveShaper();
    this.distA.oversample = '4x'; this.distB.oversample = '4x';
    this._activeShaper = 'A';
    this._curveA = new Float32Array(256); this._curveB = new Float32Array(256);
    this._buildCurve(this._curveA, this.params.distAmount);
    this._buildCurve(this._curveB, this.params.distAmount);
    this.distA.curve = this._curveA; this.distB.curve = this._curveB;
    this.distGainA = this.ctx.createGain(); this.distGainB = this.ctx.createGain();
    this.distGainA.gain.value = 1; this.distGainB.gain.value = 0;
    this.distWet = this.ctx.createGain(); this.distDry = this.ctx.createGain();
    this.distOut = this.ctx.createGain();
    this._updateDistMix();

    // === DELAY ===
    this.delay = this.ctx.createDelay(2.0);
    this.delay.delayTime.value = this.params.delayTime;
    this.delayFeedback = this.ctx.createGain();
    this.delayFeedback.gain.value = this.params.delayFeedback;
    this.delayFilter = this.ctx.createBiquadFilter();
    this.delayFilter.type = 'lowpass'; this.delayFilter.frequency.value = 3500;
    this.delayWet = this.ctx.createGain(); this.delayDry = this.ctx.createGain();
    this.delayOut = this.ctx.createGain();
    this._updateDelayMix();

    // === REVERB ===
    this.reverb = this.ctx.createConvolver();
    this._generateReverbIR();
    this.reverbWet = this.ctx.createGain(); this.reverbDry = this.ctx.createGain();
    this.reverbOut = this.ctx.createGain();
    this._updateReverbMix();

    // === MASTER + LIMITER ===
    this.master = this.ctx.createGain(); this.master.gain.value = 0.7;
    this.limiter = this.ctx.createDynamicsCompressor();
    this.limiter.threshold.value = -3; this.limiter.knee.value = 0;
    this.limiter.ratio.value = 20; this.limiter.attack.value = 0.001; this.limiter.release.value = 0.05;

    // === ANALYSER ===
    this.analyser = this.ctx.createAnalyser(); this.analyser.fftSize = 2048;

    // === WIRE SIGNAL CHAIN ===
    this.osc.connect(this.filter);
    this.subOsc.connect(this.subGain); this.subGain.connect(this.filter);
    this.filter.connect(this.vca);

    this.vca.connect(this.distDry);
    this.vca.connect(this.distA); this.vca.connect(this.distB);
    this.distA.connect(this.distGainA); this.distB.connect(this.distGainB);
    this.distGainA.connect(this.distWet); this.distGainB.connect(this.distWet);
    this.distDry.connect(this.distOut); this.distWet.connect(this.distOut);

    this.distOut.connect(this.delayDry); this.distOut.connect(this.delay);
    this.delay.connect(this.delayFilter);
    this.delayFilter.connect(this.delayFeedback); this.delayFeedback.connect(this.delay);
    this.delayFilter.connect(this.delayWet);
    this.delayDry.connect(this.delayOut); this.delayWet.connect(this.delayOut);

    this.delayOut.connect(this.reverbDry); this.delayOut.connect(this.reverb);
    this.reverb.connect(this.reverbWet);
    this.reverbDry.connect(this.reverbOut); this.reverbWet.connect(this.reverbOut);

    this.reverbOut.connect(this.master);
    this.master.connect(this.limiter);
    this.limiter.connect(this.analyser);
    this.analyser.connect(this.ctx.destination);

    this.osc.start(); this.subOsc.start();
  }

  _smooth(param, value, tc = 0.015) {
    const now = this.ctx.currentTime;
    param.cancelScheduledValues(now);
    param.setValueAtTime(param.value, now);
    param.setTargetAtTime(value, now, tc);
  }

  _buildCurve(arr, amount) { const k = amount * 2; for (let i = 0; i < arr.length; i++) { const x = (i * 2) / arr.length - 1; arr[i] = ((1 + k) * x) / (1 + k * Math.abs(x)); } }

  _updateDistortionCurve() {
    const xf = 0.02;
    if (this._activeShaper === 'A') { this._buildCurve(this._curveB, this.params.distAmount); this.distB.curve = this._curveB; this._smooth(this.distGainA.gain, 0, xf); this._smooth(this.distGainB.gain, 1, xf); this._activeShaper = 'B'; }
    else { this._buildCurve(this._curveA, this.params.distAmount); this.distA.curve = this._curveA; this._smooth(this.distGainB.gain, 0, xf); this._smooth(this.distGainA.gain, 1, xf); this._activeShaper = 'A'; }
  }

  _updateDistMix() { const on = this.params.distOn; this._smooth(this.distWet.gain, on ? 1 : 0); this._smooth(this.distDry.gain, on ? 0 : 1); }
  _updateDelayMix() { const on = this.params.delayOn; this._smooth(this.delayWet.gain, on ? this.params.delayMix : 0); this._smooth(this.delayDry.gain, 1); }
  _updateReverbMix() { const on = this.params.reverbOn; this._smooth(this.reverbWet.gain, on ? this.params.reverbMix : 0); this._smooth(this.reverbDry.gain, on ? 1 - this.params.reverbMix * 0.3 : 1); }

  _scheduleReverbIR() {
    clearTimeout(this._rvbD);
    this._rvbD = setTimeout(() => {
      this._smooth(this.reverbWet.gain, 0, 0.01);
      setTimeout(() => { this._generateReverbIR(); const t = this.params.reverbOn ? this.params.reverbMix : 0; this._smooth(this.reverbWet.gain, t, 0.02); }, 40);
    }, 80);
  }

  _generateReverbIR() {
    const r = this.ctx.sampleRate, len = r * this.params.reverbDecay;
    const ir = this.ctx.createBuffer(2, len, r);
    for (let ch = 0; ch < 2; ch++) { const d = ir.getChannelData(ch); for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, 2.5); }
    this.reverb.buffer = ir;
  }

  triggerNote(midi, time, isAccent = false, isSlide = false) {
    if (!this.ctx) return;
    const t = time || this.ctx.currentTime;
    const freq = midiToFreq(midi + this.params.tuning);

    if (isSlide && this.currentNote !== null) {
      this.osc.frequency.cancelScheduledValues(t); this.osc.frequency.setValueAtTime(this.osc.frequency.value, t);
      this.osc.frequency.linearRampToValueAtTime(freq, t + 0.06);
      this.subOsc.frequency.cancelScheduledValues(t); this.subOsc.frequency.setValueAtTime(this.subOsc.frequency.value, t);
      this.subOsc.frequency.linearRampToValueAtTime(freq / 2, t + 0.06);
    } else {
      this.osc.frequency.setValueAtTime(freq, t);
      this.subOsc.frequency.setValueAtTime(freq / 2, t);
    }

    const accentAmt = isAccent ? this.params.accent : 0;
    const baseVol = 0.3, peakVol = baseVol + accentAmt * 0.4;
    const envMod = this.params.envMod * (1 + accentAmt * 1.5);
    const decay = this.params.decay * (isAccent ? 0.7 : 1.0);

    if (!isSlide) {
      this.vca.gain.cancelScheduledValues(t);
      this.vca.gain.setValueAtTime(0.001, t);
      this.vca.gain.linearRampToValueAtTime(peakVol, t + 0.005);
      this.vca.gain.exponentialRampToValueAtTime(baseVol * 0.4 + 0.001, t + decay);
      this.vca.gain.exponentialRampToValueAtTime(0.001, t + decay + 0.1);
      this.vca.gain.setValueAtTime(0, t + decay + 0.101);

      const peakCutoff = Math.min(this.params.cutoff + envMod, 12000);
      const atk = 0.003;
      this.filterFreq.cancelScheduledValues(t);
      this.filterFreq.setValueAtTime(Math.max(this.filterFreq.value, 30), t);
      this.filterFreq.exponentialRampToValueAtTime(peakCutoff, t + atk);
      this.filterFreq.exponentialRampToValueAtTime(Math.max(this.params.cutoff, 30), t + atk + decay);
    }
    this.filterReso.setValueAtTime(this.params.resonance, t);
    this.currentNote = midi;
  }

  releaseNote(time) {
    if (!this.ctx) return;
    const t = time || this.ctx.currentTime;
    // cancelAndHoldAtTime freezes the automation value at time t without
    // a discontinuity, unlike cancelScheduledValues + setValueAtTime which
    // reads .value at JS-execution time (wrong when t is in the future).
    if (this.vca.gain.cancelAndHoldAtTime) {
      this.vca.gain.cancelAndHoldAtTime(t);
    } else {
      this.vca.gain.cancelScheduledValues(t);
      this.vca.gain.setValueAtTime(this.vca.gain.value, t);
    }
    // Smooth exponential approach to zero avoids the pop from an abrupt
    // setValueAtTime(0) and works correctly even from very small values
    // (unlike exponentialRampToValueAtTime which fails near zero).
    this.vca.gain.setTargetAtTime(0, t, 0.01);
  }

  disableScope() { if (!this.ctx) return; this.limiter.disconnect(this.analyser); this.analyser.disconnect(this.ctx.destination); this.limiter.connect(this.ctx.destination); this.scopeEnabled = false; }
  enableScope() { if (!this.ctx) return; this.limiter.disconnect(this.ctx.destination); this.limiter.connect(this.analyser); this.analyser.connect(this.ctx.destination); this.scopeEnabled = true; }

  setParam(name, value) {
    this.params[name] = value;
    if (!this.ctx) return;
    switch (name) {
      case 'cutoff': this._smooth(this.filterFreq, value); break;
      case 'resonance': this._smooth(this.filterReso, value); break;
      case 'waveform': this.osc.type = value; break;
      case 'distAmount': this._updateDistortionCurve(); break;
      case 'distOn': this._updateDistMix(); break;
      case 'delayTime': this._smooth(this.delay.delayTime, value, 0.03); break;
      case 'delayFeedback': this._smooth(this.delayFeedback.gain, value); break;
      case 'delayMix': case 'delayOn': this._updateDelayMix(); break;
      case 'reverbDecay': this._scheduleReverbIR(); break;
      case 'reverbMix': case 'reverbOn': this._updateReverbMix(); break;
      case 'lfoRate': this._smooth(this.lfo.frequency, value); break;
      case 'lfoAmount': this._smooth(this.lfoGain.gain, value); break;
      case 'lfoWave': this.lfo.type = value; break;
    }
  }
}
