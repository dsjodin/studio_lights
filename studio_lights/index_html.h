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
    max-width: 640px; margin-left: auto; margin-right: auto;
  }
  h1 { font-size: 1.4rem; margin: 0 0 16px; }
  .master { display: flex; gap: 8px; margin-bottom: 16px; }
  .master button {
    flex: 1; padding: 16px; font-size: 1.1rem;
    border: 0; border-radius: 8px; cursor: pointer;
    color: #fff; font-weight: 600;
  }
  .master .on  { background: #2a7a2a; }
  .master .off { background: #7a2a2a; }
  .master button:active { transform: translateY(1px); }
  .card {
    background: #1c1c1c; border: 1px solid #2a2a2a;
    border-radius: 10px; padding: 14px; margin-bottom: 12px;
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
    display: flex; justify-content: space-between;
    font-size: 0.9rem; color: #bbb; margin-bottom: 4px;
  }
  input[type=range] { width: 100%; }
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
let pending = false;

function el(tag, attrs, children) {
  const e = document.createElement(tag);
  for (const k in (attrs||{})) {
    if (k === 'class') e.className = attrs[k];
    else if (k.startsWith('on')) e[k] = attrs[k];
    else e.setAttribute(k, attrs[k]);
  }
  for (const c of (children||[])) {
    if (c == null) continue;
    e.appendChild(typeof c === 'string' ? document.createTextNode(c) : c);
  }
  return e;
}

function kRange(kind) {
  return kind === 'a7105' ? [3200, 5600] : [2800, 8500];
}
function chRange(kind) {
  return kind === 'a7105' ? [1, 15] : [1, 19];
}

function render() {
  const root = document.getElementById('lights');
  root.innerHTML = '';
  state.lights.forEach(L => {
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
    };
    briIn.onchange = () => setLight(L.id, {bri: parseInt(briIn.value)});
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
      };
      kIn.onchange = () => setLight(L.id, {k: parseInt(kIn.value)});
      card.appendChild(el('label', {class: 'slider'}, [kLab, kIn]));
    }

    if (showHsi) {
      const hLab = el('div', {class: 'lab'}, [
        el('span', {}, ['Hue']),
        el('span', {id: 'h-v-' + L.id}, [L.hue + 'deg'])
      ]);
      const hIn = el('input', {
        type: 'range', min: 0, max: 360, step: 1, value: L.hue
      });
      hIn.oninput = () => {
        document.getElementById('h-v-' + L.id).textContent = hIn.value + 'deg';
      };
      hIn.onchange = () => setLight(L.id, {hue: parseInt(hIn.value)});
      card.appendChild(el('label', {class: 'slider'}, [hLab, hIn]));

      const sLab = el('div', {class: 'lab'}, [
        el('span', {}, ['Saturation']),
        el('span', {id: 's-v-' + L.id}, [L.saturation + '%'])
      ]);
      const sIn = el('input', {
        type: 'range', min: 0, max: 100, step: 1, value: L.saturation
      });
      sIn.oninput = () => {
        document.getElementById('s-v-' + L.id).textContent = sIn.value + '%';
      };
      sIn.onchange = () => setLight(L.id, {sat: parseInt(sIn.value)});
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

    root.appendChild(card);
  });
}

function setStatus(s) {
  document.getElementById('status').textContent = s;
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

async function call(url, params) {
  if (pending) return;
  pending = true;
  setStatus('sending...');
  const body = new URLSearchParams(params).toString();
  try {
    const r = await fetch(url, {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body
    });
    state = await r.json();
    render();
    setStatus('ok');
  } catch (e) {
    setStatus('error');
  } finally {
    pending = false;
  }
}

function powerAll(on) {
  call('/api/power', {on});
}

function setLight(id, opts) {
  call('/api/light', Object.assign({id}, opts));
}

refresh();
</script>
</body>
</html>
)HTML";
