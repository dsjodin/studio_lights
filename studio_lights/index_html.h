#pragma once
#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Studio Lights</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body {
    margin: 0; padding: 16px;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    background: #111; color: #eee;
    max-width: 1200px; margin-left: auto; margin-right: auto;
  }
  h1 { font-size: 1.4rem; margin: 0 0 16px; }
  .master { display: flex; gap: 8px; margin-bottom: 24px; }
  .master button {
    flex: 1; padding: 16px; font-size: 1.1rem;
    border: 0; border-radius: 8px; cursor: pointer;
    color: #fff; font-weight: 600;
  }
  .master .on  { background: #2a7a2a; }
  .master .off { background: #7a2a2a; }
  .master button:active { transform: translateY(1px); }
  .group { margin-bottom: 32px; }
  .group h2 {
    font-size: 0.8rem; font-weight: 600;
    color: #888; margin: 0 0 10px;
    text-transform: uppercase; letter-spacing: 1px;
  }
  .row-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
    gap: 12px;
  }
  .card {
    background: #1c1c1c; border: 1px solid #2a2a2a;
    border-radius: 10px; padding: 14px;
  }
  .row {
    display: flex; align-items: center; justify-content: space-between;
    margin-bottom: 10px; gap: 8px;
  }
  .name {
    font-size: 1.1rem; font-weight: 600;
    background: transparent; border: 0; color: #eee;
    padding: 4px 6px; border-radius: 4px;
    flex: 1; min-width: 0;
    font-family: inherit;
  }
  .name:hover { background: #242424; }
  .name:focus { background: #2a2a2a; outline: 1px solid #555; }
  .badge {
    font-size: 0.7rem; padding: 2px 6px; border-radius: 4px;
    background: #333; color: #bbb; text-transform: uppercase;
    letter-spacing: 0.5px;
  }
  .badge.a7105    { background: #3a4a6a; color: #cde; }
  .badge.weeylite { background: #4a3a6a; color: #ecd; }
  .toggle {
    padding: 6px 14px; border: 0; border-radius: 6px; cursor: pointer;
    color: #fff; font-weight: 600;
  }
  .toggle.on  { background: #2a7a2a; }
  .toggle.off { background: #444; }
  label.slider { display: block; margin: 10px 0; }
  label.slider .lab {
    display: flex; justify-content: space-between; align-items: center;
    font-size: 0.9rem; color: #bbb; margin-bottom: 4px;
  }
  input[type=range] { width: 100%; touch-action: none; }
  input[type=range].hue {
    -webkit-appearance: none;
    appearance: none;
    height: 18px;
    border-radius: 9px;
    outline: none;
    background: linear-gradient(to right,
      #ff0000  0%,
      #ffff00 16.66%,
      #00ff00 33.33%,
      #00ffff 50%,
      #0000ff 66.66%,
      #ff00ff 83.33%,
      #ff0000 100%);
  }
  input[type=range].hue::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 22px; height: 22px;
    border-radius: 50%;
    background: #fff;
    border: 2px solid #111;
    box-shadow: 0 0 0 1px #555;
    cursor: pointer;
  }
  input[type=range].hue::-moz-range-thumb {
    width: 22px; height: 22px;
    border-radius: 50%;
    background: #fff;
    border: 2px solid #111;
    box-shadow: 0 0 0 1px #555;
    cursor: pointer;
  }
  .swatch {
    display: inline-block;
    width: 18px; height: 18px;
    border-radius: 50%;
    border: 1px solid #333;
    vertical-align: middle;
  }
  .modes { display: flex; gap: 6px; margin: 8px 0; }
  .modes button {
    flex: 1; padding: 6px; border: 0; border-radius: 5px;
    background: #333; color: #aaa; cursor: pointer; font-weight: 600;
  }
  .modes button.active { background: #555; color: #fff; }
  .cfg {
    display: flex; gap: 8px; margin-top: 8px;
    font-size: 0.85rem; color: #aaa; flex-wrap: wrap;
  }
  .cfg label { display: flex; align-items: center; gap: 4px; }
  .cfg input, .cfg select {
    background: #222; color: #eee; border: 1px solid #333;
    border-radius: 4px; padding: 2px 6px;
  }
  .cfg input { width: 50px; }
  .status { font-size: 0.8rem; color: #888; text-align: center; margin-top: 8px; }
  @media (max-width: 480px) {
    body { padding: 12px; }
    .row-grid { grid-template-columns: 1fr; }
  }
</style>
</head>
<body>
<h1>Studio Lights</h1>

<div class="master">
  <button class="on"  onclick="powerAll(1)">Power All On</button>
  <button class="off" onclick="powerAll(0)">Power All Off</button>
</div>

<div id="lights"></div>

<div class="status" id="status">loading...</div>

<script>
let state = { lights: [] };
let masterPending = false;
const lightInFlight = {};
const lightQueued   = {};
let activeSlider = null;
let renderQueued = false;

// Cap how fast we fire oninput-driven sends during a drag.
const SLIDER_THROTTLE_MS = 80;
let lastLiveSend = 0;

function el(tag, attrs, children) {
  const e = document.createElement(tag);
  for (const k in (attrs||{})) {
    if (k === 'class') e.className = attrs[k];
    else if (k === 'style') e.setAttribute('style', attrs[k]);
    else if (k.startsWith('on')) e[k] = attrs[k];
    else e.setAttribute(k, attrs[k]);
  }
  for (const c of (children||[])) {
    if (c == null) continue;
    e.appendChild(typeof c === 'string' ? document.createTextNode(c) : c);
  }
  return e;
}

function kRange(kind)  { return kind === 'a7105' ? [3200, 5600] : [2800, 8500]; }
function chRange(kind) { return kind === 'a7105' ? [1, 15] : [1, 19]; }
function swatchStyle(h, s) {
  return 'background: hsl(' + h + ', ' + (s == null ? 100 : s) + '%, 50%)';
}
// Only the Weeylite TX is non-blocking on the server, so only those
// sliders fire live on input. A7105 sends would otherwise pile up behind
// the synchronous 400 ms radio burst.
function liveCapable(L) { return L.kind === 'weeylite'; }

function trackDrag(input) {
  const stop = () => {
    if (activeSlider === input) { activeSlider = null; drainRender(); }
  };
  input.addEventListener('pointerdown',       () => { activeSlider = input; });
  input.addEventListener('pointerup',         stop);
  input.addEventListener('pointercancel',     stop);
  input.addEventListener('lostpointercapture', stop);
  input.addEventListener('touchstart', () => { activeSlider = input; }, {passive: true});
  input.addEventListener('touchend',   stop, {passive: true});
}

function liveSend(id, opts) {
  const now = Date.now();
  if (now - lastLiveSend < SLIDER_THROTTLE_MS) return;
  lastLiveSend = now;
  setLight(id, opts);
}

function makeCard(L) {
  const card = el('div', {class: 'card'});

  const nameIn = el('input', {
    class: 'name', type: 'text', value: L.name, maxlength: 31,
    'aria-label': 'Light name'
  });
  nameIn.onchange = () => {
    const v = nameIn.value.trim();
    if (v && v !== L.name) setLight(L.id, {name: v});
    else nameIn.value = L.name;
  };
  nameIn.onkeydown = (e) => { if (e.key === 'Enter') nameIn.blur(); };

  const badge = el('span', {class: 'badge ' + L.kind},
                   [L.kind === 'a7105' ? '288 RF' : 'RB9 BLE']);

  const head = el('div', {class: 'row'}, [
    nameIn,
    badge,
    el('button', {
      class: 'toggle ' + (L.power ? 'on' : 'off'),
      onclick: () => setLight(L.id, {power: L.power ? 0 : 1})
    }, [L.power ? 'ON' : 'OFF'])
  ]);
  card.appendChild(head);

  if (L.kind === 'weeylite') {
    const modes = el('div', {class: 'modes'}, [
      el('button', {
        class: L.mode === 'cct' ? 'active' : '',
        onclick: () => setLight(L.id, {mode: 'cct'})
      }, ['CCT']),
      el('button', {
        class: L.mode === 'hsi' ? 'active' : '',
        onclick: () => setLight(L.id, {mode: 'hsi'})
      }, ['HSI'])
    ]);
    card.appendChild(modes);
  }

  const briLab = el('div', {class: 'lab'}, [
    el('span', {}, ['Brightness']),
    el('span', {id: 'bri-v-' + L.id}, [L.brightness + '%'])
  ]);
  const briIn = el('input', {
    type: 'range', min: 0, max: 100, step: 1, value: L.brightness
  });
  briIn.oninput = () => {
    document.getElementById('bri-v-' + L.id).textContent = briIn.value + '%';
    if (liveCapable(L)) liveSend(L.id, {bri: parseInt(briIn.value)});
  };
  briIn.onchange = () => setLight(L.id, {bri: parseInt(briIn.value)});
  trackDrag(briIn);
  card.appendChild(el('label', {class: 'slider'}, [briLab, briIn]));

  const showCct = L.kind === 'a7105' || L.mode === 'cct';
  const showHsi = L.kind === 'weeylite' && L.mode === 'hsi';

  if (showCct) {
    const [kMin, kMax] = kRange(L.kind);
    const kLab = el('div', {class: 'lab'}, [
      el('span', {}, ['Kelvin']),
      el('span', {id: 'k-v-' + L.id}, [L.kelvin + 'K'])
    ]);
    const kIn = el('input', {
      type: 'range', min: kMin, max: kMax, step: 100, value: L.kelvin
    });
    kIn.oninput = () => {
      document.getElementById('k-v-' + L.id).textContent = kIn.value + 'K';
      if (liveCapable(L)) liveSend(L.id, {k: parseInt(kIn.value)});
    };
    kIn.onchange = () => setLight(L.id, {k: parseInt(kIn.value)});
    trackDrag(kIn);
    card.appendChild(el('label', {class: 'slider'}, [kLab, kIn]));
  }

  if (showHsi) {
    const hSwatch = el('span', {
      class: 'swatch', id: 'h-sw-' + L.id,
      style: swatchStyle(L.hue, L.saturation)
    });
    const hLab = el('div', {class: 'lab'}, [
      el('span', {}, ['Hue']),
      hSwatch
    ]);
    const hIn = el('input', {
      class: 'hue',
      type: 'range', min: 0, max: 360, step: 1, value: L.hue
    });
    hIn.oninput = () => {
      const sIn2 = document.getElementById('s-in-' + L.id);
      const sat  = sIn2 ? sIn2.value : L.saturation;
      hSwatch.setAttribute('style', swatchStyle(hIn.value, sat));
      liveSend(L.id, {hue: parseInt(hIn.value)});
    };
    hIn.onchange = () => setLight(L.id, {hue: parseInt(hIn.value)});
    trackDrag(hIn);
    card.appendChild(el('label', {class: 'slider'}, [hLab, hIn]));

    const sLab = el('div', {class: 'lab'}, [
      el('span', {}, ['Saturation']),
      el('span', {id: 's-v-' + L.id}, [L.saturation + '%'])
    ]);
    const sIn = el('input', {
      id: 's-in-' + L.id,
      type: 'range', min: 0, max: 100, step: 1, value: L.saturation
    });
    sIn.oninput = () => {
      document.getElementById('s-v-' + L.id).textContent = sIn.value + '%';
      const hIn2 = card.querySelector('input.hue');
      const hue  = hIn2 ? hIn2.value : L.hue;
      hSwatch.setAttribute('style', swatchStyle(hue, sIn.value));
      liveSend(L.id, {sat: parseInt(sIn.value)});
    };
    sIn.onchange = () => setLight(L.id, {sat: parseInt(sIn.value)});
    trackDrag(sIn);
    card.appendChild(el('label', {class: 'slider'}, [sLab, sIn]));
  }

  const [chMin, chMax] = chRange(L.kind);
  const chIn = el('input', {type: 'number', min: chMin, max: chMax, value: L.channel});
  chIn.onchange = () => setLight(L.id, {channel: parseInt(chIn.value)});

  const grpSel = el('select', {});
  const grpOpts = L.kind === 'a7105'
    ? [['A', 0], ['B', 1]]
    : [['ALL', 0], ['1', 1], ['2', 2], ['3', 3], ['4', 4], ['5', 5], ['6', 6]];
  grpOpts.forEach(([label, val]) => {
    const o = el('option', {value: val}, [label]);
    if (val === L.group) o.selected = true;
    grpSel.appendChild(o);
  });
  grpSel.onchange = () => setLight(L.id, {group: parseInt(grpSel.value)});

  const cfg = el('div', {class: 'cfg'}, [
    el('label', {}, ['Ch', chIn]),
    el('label', {}, ['Grp', grpSel])
  ]);
  card.appendChild(cfg);

  return card;
}

function makeGroup(title, lights) {
  const grid = el('div', {class: 'row-grid'});
  lights.forEach(L => grid.appendChild(makeCard(L)));
  return el('section', {class: 'group'}, [
    el('h2', {}, [title]),
    grid
  ]);
}

function render() {
  const root = document.getElementById('lights');
  root.innerHTML = '';
  const a7105 = state.lights.filter(L => L.kind === 'a7105');
  const weey  = state.lights.filter(L => L.kind === 'weeylite');
  if (a7105.length) root.appendChild(makeGroup('288ARC',        a7105));
  if (weey.length)  root.appendChild(makeGroup('Weeylite RB9',  weey));
}

// Rebuilding the DOM while the user is dragging a slider would tear the
// pointer-captured element out from under them. Defer renders until the
// drag releases.
function maybeRender() {
  if (activeSlider) { renderQueued = true; return; }
  render();
}
function drainRender() {
  if (renderQueued) { renderQueued = false; render(); }
}

function setStatus(s) {
  if (activeSlider) return;
  document.getElementById('status').textContent = s;
}

function setLight(id, opts) {
  lightQueued[id] = Object.assign(lightQueued[id] || {}, opts);
  flushLight(id);
}

async function flushLight(id) {
  if (lightInFlight[id] || !lightQueued[id]) return;
  const params = lightQueued[id];
  lightQueued[id]   = null;
  lightInFlight[id] = true;
  setStatus('sending...');
  try {
    const r = await fetch('/api/light', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: new URLSearchParams(Object.assign({id: id}, params))
    });
    state = await r.json();
    maybeRender();
    setStatus('ok');
  } catch (e) {
    setStatus('error');
  } finally {
    lightInFlight[id] = false;
    if (lightQueued[id]) flushLight(id);
  }
}

async function powerAll(on) {
  if (masterPending) return;
  masterPending = true;
  setStatus('sending...');
  try {
    const r = await fetch('/api/power', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: new URLSearchParams({on: on})
    });
    state = await r.json();
    maybeRender();
    setStatus('ok');
  } catch (e) {
    setStatus('error');
  } finally {
    masterPending = false;
  }
}

async function refresh() {
  try {
    const r = await fetch('/api/state');
    state = await r.json();
    render();
    const r_ok = state.radio_ok ? 'A7105 ok' : 'A7105 down';
    const b_ok = state.ble_ok   ? 'BLE ok'   : 'BLE down';
    setStatus(r_ok + ' / ' + b_ok);
  } catch (e) {
    setStatus('offline');
  }
}

refresh();
</script>
</body>
</html>
)HTML";
