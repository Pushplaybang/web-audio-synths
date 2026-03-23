// 4-pole diode ladder filter with per-stage tanh saturation.
// Runs on the audio thread — replaces BiquadFilterNode with a
// physically-modeled analog filter that can't become unstable.
class DiodeLadderProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: 'frequency', defaultValue: 800, minValue: 20, maxValue: 18000, automationRate: 'a-rate' },
      { name: 'resonance', defaultValue: 8, minValue: 0, maxValue: 30, automationRate: 'a-rate' },
      { name: 'detune', defaultValue: 0, minValue: -4800, maxValue: 4800, automationRate: 'a-rate' },
    ];
  }
  constructor() { super(); this.s = new Float64Array(4); }
  process(inputs, outputs, parameters) {
    const inp = inputs[0] && inputs[0][0];
    const out = outputs[0] && outputs[0][0];
    if (!inp || !out) return true;
    const fA = parameters.frequency, rA = parameters.resonance, dA = parameters.detune;
    const fK = fA.length === 1, rK = rA.length === 1, dK = dA.length === 1;
    const sr = sampleRate, ny = sr * 0.45, PI = 3.141592653589793;
    for (let i = 0; i < out.length; i++) {
      let freq = (fK ? fA[0] : fA[i]) * Math.pow(2, (dK ? dA[0] : dA[i]) / 1200);
      if (freq < 30) freq = 30; if (freq > ny) freq = ny;
      const g = 2 * Math.tan(PI * freq / sr); const G = g / (1 + g);
      const k = ((rK ? rA[0] : rA[i]) / 30) * 3.8;
      const fb = k * this.t(this.s[3]);
      let x = this.t(inp[i] - fb);
      this.s[0] += G * (x - this.s[0]); x = this.t(this.s[0]);
      this.s[1] += G * (x - this.s[1]); x = this.t(this.s[1]);
      this.s[2] += G * (x - this.s[2]); x = this.t(this.s[2]);
      this.s[3] += G * (x - this.s[3]);
      out[i] = this.s[3];
    }
    return true;
  }
  t(x) { if (x > 3) return 1; if (x < -3) return -1; const x2 = x*x; return x*(27+x2)/(27+9*x2); }
}
registerProcessor('diode-ladder', DiodeLadderProcessor);
