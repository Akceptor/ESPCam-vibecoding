#pragma once
#include <pgmspace.h>

// Main controller page — stored in flash (PROGMEM) to save RAM.
// Layout (landscape): [LEFT STICK] [VIDEO] [RIGHT STICK]
//                               [CONTROLS]
// Layout (portrait):  [VIDEO — full width]
//                     [LEFT STICK] [RIGHT STICK]
//                     [ARM / FM / STATUS / CFG — full width row]
// Left stick:  Throttle (Y, up=+) / Yaw (X)
// Right stick: Roll (X) / Pitch (Y, up=+)
// Both sticks snap to center on release.

static const char MAIN_PAGE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Drone Controller</title>
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  html, body {
    background: #0d0d0d;
    color: #e0e0e0;
    font-family: system-ui, -apple-system, sans-serif;
    height: 100%;
    overflow: hidden;
    touch-action: none;
    user-select: none;
    -webkit-user-select: none;
  }

  /* ── Landscape layout (default) ────────────────────────────────── */
  #app {
    display: grid;
    grid-template-columns: 38vw 1fr 38vw;
    grid-template-rows: 1fr auto;
    width: 100vw;
    height: 100vh;
  }
  #left-stick  { grid-column: 1; grid-row: 1 / 3; }
  #stream-wrap { grid-column: 2; grid-row: 1; min-height: 0; }
  #ctrl-col    { grid-column: 2; grid-row: 2; }
  #right-stick { grid-column: 3; grid-row: 1 / 3; }

  /* ── Portrait layout ────────────────────────────────────────────── */
  @media (orientation: portrait) {
    #app {
      grid-template-columns: 1fr 1fr;
      /* Fixed stream row so video is ALWAYS visible regardless of stick/ctrl size */
      grid-template-rows: 30vh 1fr auto;
    }
    #stream-wrap { grid-column: 1 / -1; grid-row: 1; padding: 4px 6px 2px; }
    #stream      { max-width: 100%; max-height: calc(30vh - 10px); }
    #left-stick  { grid-column: 1;      grid-row: 2; }
    #right-stick { grid-column: 2;      grid-row: 2; }
    #ctrl-col    { grid-column: 1 / -1; grid-row: 3;
                   flex-direction: row;
                   justify-content: center;
                   flex-wrap: wrap;
                   gap: 6px 10px;
                   padding: 6px 10px 12px; }
    #arm-btn     { width: auto; padding: 8px 18px; font-size: 14px; }
    .fm-row      { width: auto; }
    .fm-btn      { padding: 8px 10px; }
  }

  /* ── Shared elements ────────────────────────────────────────────── */
  .stick-col {
    display: flex;
    align-items: center;
    justify-content: center;
  }
  canvas.stick {
    border-radius: 50%;
    touch-action: none;
    display: block;
  }

  #stream-wrap {
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 6px 2px 3px;
    position: relative;
  }
  #stream {
    max-width: 100%;
    max-height: 52vh;
    object-fit: contain;
    border: 1px solid #2a2a2a;
    background: #000;
    display: block;
  }
  #res-badge {
    position: absolute;
    bottom: 10px;
    right: 8px;
    background: rgba(0,0,0,0.55);
    color: #666;
    font-size: 9px;
    font-family: monospace;
    padding: 2px 5px;
    border-radius: 3px;
    pointer-events: none;
    letter-spacing: 0.5px;
  }
  #rssi-badge {
    position: absolute;
    bottom: 10px;
    left: 8px;
    background: rgba(0,0,0,0.55);
    font-size: 9px;
    font-family: monospace;
    padding: 2px 5px;
    border-radius: 3px;
    pointer-events: none;
    letter-spacing: 0.5px;
  }

  #ctrl-col {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 6px;
    padding: 4px 2px 10px;
  }
  #arm-btn {
    width: min(88%, 140px);
    padding: 9px 0;
    font-size: 15px;
    font-weight: 700;
    letter-spacing: 1px;
    border: none;
    border-radius: 8px;
    cursor: pointer;
    background: #6b0000;
    color: #fff;
    transition: background 0.15s;
    -webkit-tap-highlight-color: transparent;
  }
  #arm-btn.armed { background: #005f00; }
  .fm-row {
    display: flex;
    gap: 5px;
    width: min(88%, 140px);
  }
  .fm-btn {
    flex: 1;
    padding: 7px 4px;
    font-size: 11px;
    font-weight: 600;
    border: none;
    border-radius: 6px;
    background: #222;
    color: #666;
    cursor: pointer;
    transition: all 0.12s;
    -webkit-tap-highlight-color: transparent;
  }
  .fm-btn.active { background: #1a4080; color: #fff; }
  #status {
    font-size: 10px;
    color: #444;
    transition: color 0.3s;
  }
  #status.ok  { color: #3a9; }
  #status.err { color: #a33; }
  #cfg-link {
    font-size: 12px;
    color: #4af;
    text-decoration: none;
    padding: 4px 10px;
    border: 1px solid #2a4a6a;
    border-radius: 5px;
  }
  #cfg-link:hover { background: #1a2a3a; }
  .label {
    font-size: 9px;
    color: #444;
    text-align: center;
    margin-top: 2px;
  }
</style>
</head>
<body>
<div id="app">
  <!-- Video stream — top-center in landscape, full-width top in portrait -->
  <div id="stream-wrap">
    <img id="stream" src="http://192.168.0.1:81/stream" alt="FPV">
    <div id="rssi-badge"></div>
    <div id="res-badge"></div>
  </div>

  <!-- Left joystick: Throttle / Yaw -->
  <div class="stick-col" id="left-stick">
    <div>
      <canvas id="lStick" class="stick"></canvas>
      <div class="label">THR &nbsp;&#8593;&nbsp; / &nbsp;YAW &#8592;&#8594;</div>
    </div>
  </div>

  <!-- Center controls -->
  <div id="ctrl-col">
    <button id="arm-btn" onclick="toggleArm()">DISARMED</button>
    <div class="fm-row">
      <button class="fm-btn active" id="fm0" onclick="setFm(0)">STB</button>
      <button class="fm-btn"        id="fm1" onclick="setFm(1)">ACRO</button>
      <button class="fm-btn"        id="fm2" onclick="setFm(2)">AUTO</button>
    </div>
    <div id="status">Connecting&hellip;</div>
    <a id="cfg-link" href="/config">&#9881; Config</a>
  </div>

  <!-- Right joystick: Roll / Pitch -->
  <div class="stick-col" id="right-stick">
    <div>
      <canvas id="rStick" class="stick"></canvas>
      <div class="label">ROLL &#8592;&#8594; / &nbsp;PCH &nbsp;&#8593;&nbsp;</div>
    </div>
  </div>
</div>

<script>
// ── State ─────────────────────────────────────────────────────────────────────
let lx = 0, ly = 0;  // left stick  (yaw / throttle)
let rx = 0, ry = 0;  // right stick (roll / pitch)
let armed = false;
let fm = 0;

// ── WebSocket ─────────────────────────────────────────────────────────────────
let ws, wsOk = false;

function connectWs() {
  try {
    ws = new WebSocket('ws://' + location.hostname + '/ws');
  } catch(e) { return; }
  ws.onopen  = () => { wsOk = true;  setStatus(true);  };
  ws.onclose = () => { wsOk = false; setStatus(false); setTimeout(connectWs, 2000); };
  ws.onerror = () => ws.close();
  ws.onmessage = e => {
    try {
      const d = JSON.parse(e.data);
      if (d.rssi !== undefined) updateRssi(d.rssi);
      if (d.fps  !== undefined) updateFps(d.fps);
    } catch(_) {}
  };
}
connectWs();

function setStatus(ok) {
  const el = document.getElementById('status');
  el.textContent = ok ? 'Connected' : 'Disconnected';
  el.className   = ok ? 'ok' : 'err';
}

function updateRssi(dbm) {
  const el = document.getElementById('rssi-badge');
  if (!el) return;
  const filled = dbm >= -55 ? 4 : dbm >= -65 ? 3 : dbm >= -75 ? 2 : 1;
  el.textContent = '\u2588'.repeat(filled) + '\u00b7'.repeat(4 - filled) + ' ' + dbm;
  el.style.color = dbm >= -65 ? '#4c4' : dbm >= -75 ? '#ca4' : '#c44';
}

// Send at 50 Hz — server applies its own 100 Hz CRSF timer
function sendState() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({
      t: Math.round(-ly),   // up   = positive throttle
      y: Math.round(lx),
      r: Math.round(rx),
      p: Math.round(-ry),   // up   = positive pitch
      a: armed ? 1 : 0,
      f: fm
    }));
  }
}
setInterval(sendState, 20);

// ── Arm & Flight-mode ─────────────────────────────────────────────────────────
function toggleArm() {
  armed = !armed;
  const btn = document.getElementById('arm-btn');
  btn.textContent = armed ? 'ARMED' : 'DISARMED';
  btn.className   = armed ? 'armed' : '';
}

function setFm(v) {
  fm = v;
  for (let i = 0; i < 3; i++) {
    const b = document.getElementById('fm' + i);
    b.className = 'fm-btn' + (i === v ? ' active' : '');
  }
}

// ── Joystick ──────────────────────────────────────────────────────────────────
function Joystick(canvasId, onChange) {
  const canvas = document.getElementById(canvasId);
  const ctx    = canvas.getContext('2d');
  let touchId  = null;      // active touch identifier, null when released
  let mouseDown = false;
  let jx = 0, jy = 0;      // current position -100..100

  // ---- Sizing ---------------------------------------------------------------
  function resize() {
    const col = canvas.parentElement.parentElement; // .stick-col
    const size = Math.floor(Math.min(col.clientWidth * 0.90,
                                     col.clientHeight * 0.80));
    canvas.width  = size;
    canvas.height = size;
    draw();
  }
  resize();
  window.addEventListener('resize',            resize);
  window.addEventListener('orientationchange', resize);

  // ---- Drawing --------------------------------------------------------------
  function draw() {
    const w  = canvas.width;
    const cr = w / 2;        // canvas centre
    const kr = cr * 0.21;    // knob radius
    const mt = cr - kr - 6;  // max travel from centre

    ctx.clearRect(0, 0, w, w);

    // Base circle
    const baseGrad = ctx.createRadialGradient(cr, cr, cr * 0.3, cr, cr, cr);
    baseGrad.addColorStop(0, '#1e1e1e');
    baseGrad.addColorStop(1, '#141414');
    ctx.fillStyle = baseGrad;
    ctx.beginPath();
    ctx.arc(cr, cr, cr - 2, 0, 2 * Math.PI);
    ctx.fill();

    // Outer ring
    ctx.strokeStyle = '#2e2e2e';
    ctx.lineWidth   = 1.5;
    ctx.beginPath();
    ctx.arc(cr, cr, cr - 2, 0, 2 * Math.PI);
    ctx.stroke();

    // Cross-hair
    ctx.strokeStyle = '#252525';
    ctx.lineWidth   = 1;
    ctx.beginPath();
    ctx.moveTo(cr, 10); ctx.lineTo(cr, w - 10);
    ctx.moveTo(10, cr); ctx.lineTo(w - 10, cr);
    ctx.stroke();

    // Inner guide circle
    ctx.strokeStyle = '#202020';
    ctx.lineWidth   = 1;
    ctx.beginPath();
    ctx.arc(cr, cr, mt, 0, 2 * Math.PI);
    ctx.stroke();

    // Knob
    const kx = cr + jx * mt / 100;
    const ky = cr + jy * mt / 100;
    const kg = ctx.createRadialGradient(kx - kr * 0.3, ky - kr * 0.3, kr * 0.05,
                                        kx, ky, kr);
    kg.addColorStop(0, '#6ad');
    kg.addColorStop(1, '#25688a');
    ctx.fillStyle = kg;
    ctx.beginPath();
    ctx.arc(kx, ky, kr, 0, 2 * Math.PI);
    ctx.fill();
    ctx.strokeStyle = '#4af';
    ctx.lineWidth   = 1;
    ctx.stroke();
  }

  // ---- Coordinate helper ---------------------------------------------------
  function eventToJoystick(clientX, clientY) {
    const rect = canvas.getBoundingClientRect();
    const scX  = canvas.width  / rect.width;
    const scY  = canvas.height / rect.height;
    const cr   = canvas.width / 2;
    const kr   = cr * 0.21;
    const mt   = cr - kr - 6;

    let x = ((clientX - rect.left) * scX - cr) / mt * 100;
    let y = ((clientY - rect.top)  * scY - cr) / mt * 100;
    const len = Math.hypot(x, y);
    if (len > 100) { x = x / len * 100; y = y / len * 100; }
    return { x, y };
  }

  function findTouch(touchList, id) {
    for (let i = 0; i < touchList.length; i++)
      if (touchList[i].identifier === id) return touchList[i];
    return null;
  }

  // ---- Touch ---------------------------------------------------------------
  canvas.addEventListener('touchstart', e => {
    e.preventDefault();
    if (touchId === null) {
      const t  = e.changedTouches[0];
      touchId  = t.identifier;
      const xy = eventToJoystick(t.clientX, t.clientY);
      jx = xy.x; jy = xy.y;
      draw(); onChange(jx, jy);
    }
  }, { passive: false });

  canvas.addEventListener('touchmove', e => {
    e.preventDefault();
    const t = findTouch(e.targetTouches, touchId);
    if (t) {
      const xy = eventToJoystick(t.clientX, t.clientY);
      jx = xy.x; jy = xy.y;
      draw(); onChange(jx, jy);
    }
  }, { passive: false });

  canvas.addEventListener('touchend', e => {
    e.preventDefault();
    if (findTouch(e.changedTouches, touchId)) {
      touchId = null; jx = 0; jy = 0;
      draw(); onChange(0, 0);
    }
  }, { passive: false });

  canvas.addEventListener('touchcancel', e => {
    e.preventDefault();
    touchId = null; jx = 0; jy = 0;
    draw(); onChange(0, 0);
  }, { passive: false });

  // ---- Mouse (desktop testing) --------------------------------------------
  canvas.addEventListener('mousedown', e => {
    e.preventDefault();
    mouseDown = true;
    const xy = eventToJoystick(e.clientX, e.clientY);
    jx = xy.x; jy = xy.y;
    draw(); onChange(jx, jy);
  });
  window.addEventListener('mousemove', e => {
    if (!mouseDown) return;
    const xy = eventToJoystick(e.clientX, e.clientY);
    jx = xy.x; jy = xy.y;
    draw(); onChange(jx, jy);
  });
  window.addEventListener('mouseup', () => {
    if (mouseDown) {
      mouseDown = false; jx = 0; jy = 0;
      draw(); onChange(0, 0);
    }
  });
}

new Joystick('lStick', (x, y) => { lx = x; ly = y; });
new Joystick('rStick', (x, y) => { rx = x; ry = y; });

// Resolution badge — shared helper; updated by both fetch and FPS broadcasts
const RES_NAMES = {1:'QQVGA', 5:'QVGA', 8:'VGA'};
let lastRes = '', lastFps = null;

function updateResBadge() {
  const el = document.getElementById('res-badge');
  if (!el) return;
  el.textContent = lastRes + (lastFps !== null ? ' \u00b7 ' + lastFps + 'fps' : '');
}

function updateFps(fps) {
  lastFps = fps;
  updateResBadge();
}

fetch('/config/data').then(r => r.json()).then(d => {
  lastRes = RES_NAMES[d.resolution] || ('res:' + d.resolution);
  updateResBadge();
}).catch(() => {});
</script>
</body>
</html>
)rawhtml";
