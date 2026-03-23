// Sample-accurate sequencer clock on the audio thread.
// Counts samples and posts step events with precise timestamps.
class SeqClockProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: 'tempo', defaultValue: 138, minValue: 40, maxValue: 300, automationRate: 'k-rate' },
      { name: 'playing', defaultValue: 0, minValue: 0, maxValue: 1, automationRate: 'k-rate' },
    ];
  }
  constructor() { super(); this.sc = 0; this.cs = -1;
    this.port.onmessage = (e) => { if (e.data.type === 'reset') { this.sc = 0; this.cs = -1; } };
  }
  process(inputs, outputs, parameters) {
    const out = outputs[0] && outputs[0][0]; if (out) out.fill(0);
    const tempo = parameters.tempo[0], playing = parameters.playing[0];
    if (playing < 0.5) {
      if (this.cs !== -1) { this.cs = -1; this.sc = 0; this.port.postMessage({ type: 'stop' }); }
      return true;
    }
    const sps = (60 / tempo / 4) * sampleRate;
    for (let i = 0; i < 128; i++) {
      const step = Math.floor(this.sc / sps) % 16;
      if (step !== this.cs) {
        this.cs = step;
        this.port.postMessage({ type: 'step', step, time: currentTime + i / sampleRate, stepDuration: sps / sampleRate });
      }
      this.sc++;
    }
    return true;
  }
}
registerProcessor('seq-clock', SeqClockProcessor);
