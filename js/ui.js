// ============================================================
// UI
// ============================================================
class UI {
  constructor(synth, seq) {
    this.synth = synth; this.seq = seq;
    this.editMode = 'gate'; this.selectedStep = null;
    this._initKnobs(); this._initToggles(); this._initWaveformButtons();
    this._initTransport(); this._initSequencer(); this._initKeyboard(); this._initScope();
  }

  _initKnobs() {
    document.querySelectorAll('.knob-container').forEach(knob => {
      const min = +knob.dataset.min, max = +knob.dataset.max, val = +knob.dataset.value;
      const curve = knob.dataset.curve || 'linear';
      knob._norm = curve === 'exp' ? Math.log(val/min)/Math.log(max/min) : (val-min)/(max-min);
      this._updateKnobVisual(knob);
      let startY, startNorm;
      const onMove = (e) => {
        const cy = e.touches ? e.touches[0].clientY : e.clientY;
        knob._norm = Math.max(0, Math.min(1, startNorm + (startY - cy) / 200));
        let nv = curve === 'exp' ? min * Math.pow(max/min, knob._norm) : min + (max-min) * knob._norm;
        if (knob.dataset.step) nv = Math.round(nv / +knob.dataset.step) * +knob.dataset.step;
        knob.dataset.value = nv;
        this.synth.setParam(knob.dataset.param, nv);
        this._updateKnobVisual(knob); this._updateKnobValue(knob, nv);
      };
      const onUp = () => { document.removeEventListener('mousemove', onMove); document.removeEventListener('mouseup', onUp); document.removeEventListener('touchmove', onMove); document.removeEventListener('touchend', onUp); };
      knob.addEventListener('mousedown', (e) => { e.preventDefault(); startY = e.clientY; startNorm = knob._norm; document.addEventListener('mousemove', onMove); document.addEventListener('mouseup', onUp); });
      knob.addEventListener('touchstart', (e) => { e.preventDefault(); startY = e.touches[0].clientY; startNorm = knob._norm; document.addEventListener('touchmove', onMove, {passive:false}); document.addEventListener('touchend', onUp); });
      knob.addEventListener('dblclick', () => { const d=+knob.dataset.value; knob._norm = curve==='exp' ? Math.log(d/min)/Math.log(max/min) : (d-min)/(max-min); this.synth.setParam(knob.dataset.param, d); this._updateKnobVisual(knob); this._updateKnobValue(knob, d); });
    });
  }

  _updateKnobVisual(k) {
    k.querySelector('.knob-cap').style.transform = `rotate(${-135 + k._norm * 270}deg)`;
    const a = k._norm * 270;
    k.querySelector('.knob-ring').style.background = `conic-gradient(from 225deg, var(--accent) 0deg, var(--accent) ${a}deg, var(--border) ${a}deg, var(--border) 270deg, transparent 270deg)`;
  }

  _updateKnobValue(k, v) {
    const el = k.parentElement.querySelector('.knob-value'); if (!el) return;
    const p = k.dataset.param;
    if (p==='tuning') el.textContent = v>0?`+${v}`:v;
    else if (p==='lfoRate') el.textContent = `${v.toFixed(1)} Hz`;
    else if (p==='lfoAmount') el.textContent = `${Math.round(v)} ct`;
    else if (p==='delayTime') el.textContent = `${Math.round(v*1000)}ms`;
    else if (p==='reverbDecay') el.textContent = `${v.toFixed(1)}s`;
    else if (p==='distAmount'||p==='cutoff'||p==='envMod') el.textContent = Math.round(v);
    else el.textContent = v.toFixed(2);
  }

  _initToggles() {
    document.querySelectorAll('.toggle-switch').forEach(t => {
      t.addEventListener('click', async () => { await this.synth.init(); t.classList.toggle('active'); this.synth.setParam(t.dataset.toggle, t.classList.contains('active')); });
    });
  }

  _initWaveformButtons() {
    const ob = document.querySelectorAll('.wave-btn:not(.lfo-wave)');
    ob.forEach(b => b.addEventListener('click', async () => { await this.synth.init(); ob.forEach(x=>x.classList.remove('active')); b.classList.add('active'); this.synth.setParam('waveform', b.dataset.wave); }));
    const lb = document.querySelectorAll('.lfo-wave');
    lb.forEach(b => b.addEventListener('click', async () => { await this.synth.init(); lb.forEach(x=>x.classList.remove('active')); b.classList.add('active'); this.synth.setParam('lfoWave', b.dataset.lfoWave); }));
  }

  _initTransport() {
    const playBtn = document.getElementById('playBtn');
    document.getElementById('playBtn').addEventListener('click', async () => {
      await this.synth.init(); await this.seq.init();
      if (!this.seq.isPlaying) { this.seq.start(); playBtn.classList.add('active'); }
    });
    document.getElementById('stopBtn').addEventListener('click', () => { this.seq.stop(); playBtn.classList.remove('active'); this._clearHL(); });
    document.getElementById('randomBtn').addEventListener('click', () => { this.seq.randomize(); this._renderSeq(); });
    document.getElementById('tempoUp').addEventListener('click', () => { this.seq.tempo = Math.min(300, this.seq.tempo + 2); document.getElementById('tempoDisplay').textContent = this.seq.tempo; });
    document.getElementById('tempoDown').addEventListener('click', () => { this.seq.tempo = Math.max(40, this.seq.tempo - 2); document.getElementById('tempoDisplay').textContent = this.seq.tempo; });
    this.seq.onStep = (s) => this._hlStep(s);
  }

  _initSequencer() {
    const nr = document.getElementById('stepNumbers');
    for (let i = 0; i < 16; i++) { const d = document.createElement('div'); d.className='step-num'; d.textContent=i+1; nr.appendChild(d); }
    this._renderSeq();
    document.querySelectorAll('.seq-mode-btn').forEach(b => b.addEventListener('click', () => {
      if (b.dataset.mode==='clear') { this.seq.clear(); this._renderSeq(); return; }
      document.querySelectorAll('.seq-mode-btn').forEach(x=>x.classList.remove('active'));
      b.classList.add('active'); this.editMode = b.dataset.mode; this._renderSeq();
    }));
  }

  _renderSeq() {
    const g = document.getElementById('stepGrid'), n = document.getElementById('noteDisplay');
    g.innerHTML = ''; n.innerHTML = '';
    for (let i = 0; i < 16; i++) {
      const s = this.seq.steps[i];
      const btn = document.createElement('div'); btn.className = 'step-btn';
      if (this.editMode==='gate'&&s.gate) btn.classList.add('gate-on');
      if (this.editMode==='accent'&&s.accent) btn.classList.add('accent-on');
      if (this.editMode==='slide'&&s.slide) btn.classList.add('slide-on');
      btn.addEventListener('click', () => { const st=this.seq.steps[i]; if(this.editMode==='gate')st.gate=!st.gate; else if(this.editMode==='accent')st.accent=!st.accent; else st.slide=!st.slide; this._renderSeq(); });
      g.appendChild(btn);
      const nc = document.createElement('div'); nc.className = 'note-cell';
      if (this.selectedStep===i) nc.classList.add('editing');
      nc.textContent = noteToName(s.note);
      nc.addEventListener('click', () => { this.selectedStep = this.selectedStep===i ? null : i; this._renderSeq(); });
      n.appendChild(nc);
    }
  }

  _hlStep(s) { document.querySelectorAll('.step-btn').forEach((b,i)=>b.classList.toggle('current',i===s)); }
  _clearHL() { document.querySelectorAll('.step-btn').forEach(b=>b.classList.remove('current')); }

  _initKeyboard() {
    const km = {'z':0,'s':1,'x':2,'d':3,'c':4,'v':5,'g':6,'b':7,'h':8,'n':9,'j':10,'m':11,',':12};
    document.addEventListener('keydown', async (e) => {
      if (e.repeat) return; const k = e.key.toLowerCase();
      if (k===' ') { e.preventDefault(); await this.synth.init(); await this.seq.init();
        if (this.seq.isPlaying) { this.seq.stop(); document.getElementById('playBtn').classList.remove('active'); this._clearHL(); }
        else { this.seq.start(); document.getElementById('playBtn').classList.add('active'); } return; }
      if (this.selectedStep!==null) {
        if (k==='arrowup') { e.preventDefault(); this.seq.steps[this.selectedStep].note=Math.min(72,this.seq.steps[this.selectedStep].note+1); this._renderSeq(); return; }
        if (k==='arrowdown') { e.preventDefault(); this.seq.steps[this.selectedStep].note=Math.max(24,this.seq.steps[this.selectedStep].note-1); this._renderSeq(); return; }
      }
      if (k in km) { await this.synth.init(); const m=48+km[k];
        if (this.selectedStep!==null) { this.seq.steps[this.selectedStep].note=m; this.seq.steps[this.selectedStep].gate=true; this.selectedStep=(this.selectedStep+1)%16; this._renderSeq(); }
        this.synth.triggerNote(m); }
    });
    document.addEventListener('keyup', (e) => { if (e.key.toLowerCase() in km) this.synth.releaseNote(); });
  }

  _initScope() {
    const canvas = document.getElementById('scopeCanvas'), ctx = canvas.getContext('2d');
    const container = canvas.closest('.scope-container'), toggle = document.getElementById('scopeToggle');
    const accent = getComputedStyle(document.documentElement).getPropertyValue('--accent').trim();
    this.scopeActive = true; let rafId = null, buf = null;
    const draw = () => {
      if (!this.scopeActive) { rafId = null; return; }
      rafId = requestAnimationFrame(draw);
      const r = canvas.getBoundingClientRect();
      canvas.width = r.width * devicePixelRatio; canvas.height = r.height * devicePixelRatio;
      ctx.scale(devicePixelRatio, devicePixelRatio);
      const w = r.width, h = r.height;
      ctx.clearRect(0,0,w,h);
      ctx.strokeStyle = 'rgba(255,87,34,0.06)'; ctx.lineWidth = 0.5;
      for (let y=0;y<h;y+=h/4) { ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(w,y); ctx.stroke(); }
      if (!this.synth.analyser||!this.synth.scopeEnabled) return;
      const len = this.synth.analyser.fftSize;
      if (!buf||buf.length!==len) buf = new Float32Array(len);
      this.synth.analyser.getFloatTimeDomainData(buf);
      ctx.strokeStyle = accent; ctx.lineWidth = 1.5; ctx.shadowColor = accent; ctx.shadowBlur = 4;
      ctx.beginPath(); const sw = w/len; let x = 0;
      for (let i=0;i<len;i++) { const y=(1-buf[i])*h/2; i===0?ctx.moveTo(x,y):ctx.lineTo(x,y); x+=sw; }
      ctx.stroke(); ctx.shadowBlur = 0;
    };
    toggle.addEventListener('click', async () => {
      await this.synth.init(); this.scopeActive = !this.scopeActive;
      toggle.classList.toggle('active', this.scopeActive); container.classList.toggle('disabled', !this.scopeActive);
      if (this.scopeActive) { this.synth.enableScope(); if (!rafId) draw(); }
      else { this.synth.disableScope(); const r=canvas.getBoundingClientRect(); canvas.width=r.width*devicePixelRatio; canvas.height=r.height*devicePixelRatio; ctx.clearRect(0,0,canvas.width,canvas.height); }
    });
    draw();
  }
}
