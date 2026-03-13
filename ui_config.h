#pragma once
#include <pgmspace.h>

static const char CONFIG_PAGE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-CAM Config</title>
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: #0d0d0d;
    color: #e0e0e0;
    font-family: system-ui, -apple-system, sans-serif;
    max-width: 520px;
    margin: 0 auto;
    padding: 20px 16px 40px;
  }
  h1 { font-size: 18px; color: #4af; margin-bottom: 20px; }
  h2 { font-size: 13px; color: #888; margin: 20px 0 10px; text-transform: uppercase; letter-spacing: 1px; }
  .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
  .field { display: flex; flex-direction: column; gap: 4px; }
  label { font-size: 11px; color: #777; }
  input[type=number] {
    width: 100%;
    padding: 9px 10px;
    background: #1a1a1a;
    border: 1px solid #2e2e2e;
    border-radius: 5px;
    color: #e0e0e0;
    font-size: 15px;
  }
  input[type=number]:focus {
    outline: none;
    border-color: #4af;
  }
  select {
    width: 100%;
    padding: 9px 10px;
    background: #1a1a1a;
    border: 1px solid #2e2e2e;
    border-radius: 5px;
    color: #e0e0e0;
    font-size: 15px;
  }
  select:focus { outline: none; border-color: #4af; }
  .note {
    font-size: 10px;
    color: #555;
    margin-top: 2px;
    line-height: 1.4;
  }
  .note.warn { color: #884; }
  .actions { margin-top: 24px; display: flex; flex-direction: column; gap: 8px; }
  button {
    width: 100%;
    padding: 11px;
    border: none;
    border-radius: 6px;
    font-size: 14px;
    font-weight: 600;
    cursor: pointer;
    transition: background 0.15s;
  }
  #save-btn { background: #1a4080; color: #fff; }
  #save-btn:hover { background: #255aaa; }
  #save-btn:disabled { background: #2a2a2a; color: #555; cursor: default; }
  #back-btn { background: #1a1a1a; color: #aaa; border: 1px solid #2e2e2e; }
  #back-btn:hover { background: #242424; }
  #msg {
    margin-top: 14px;
    padding: 10px 12px;
    border-radius: 5px;
    font-size: 13px;
    display: none;
  }
  #msg.ok  { background: #0a3a0a; color: #4c4; display: block; }
  #msg.err { background: #3a0a0a; color: #e55; display: block; }
  .divider { border: none; border-top: 1px solid #1e1e1e; margin: 8px 0; }
</style>
</head>
<body>
<h1>&#9881; Configuration</h1>

<h2>Serial Pins</h2>
<div class="grid">
  <div class="field">
    <label>TX Pin (ESP32 &#8594; FC)</label>
    <input type="number" id="tx_pin" min="0" max="39" value="14">
    <div class="note">GPIO 14 = SD_CLK. Not a strapping pin; safe when SD card is not connected.</div>
  </div>
  <div class="field">
    <label>RX Pin (FC &#8594; ESP32)</label>
    <input type="number" id="rx_pin" min="0" max="39" value="13">
    <div class="note">Telemetry from flight controller (optional).</div>
  </div>
</div>

<hr class="divider" style="margin-top:20px;">
<h2>Camera</h2>
<div class="field">
  <label>Resolution</label>
  <select id="resolution">
    <option value="1">160&times;120 &nbsp;(QQVGA &mdash; fastest)</option>
    <option value="5" selected>320&times;240 &nbsp;(QVGA &mdash; default)</option>
    <option value="8">640&times;480 &nbsp;(VGA &mdash; higher detail)</option>
  </select>
  <div class="note">Takes effect after reboot. Lower resolution = higher frame rate.</div>
</div>

<hr class="divider" style="margin-top:20px;">
<h2>Channel Mapping (1 &ndash; 16)</h2>
<div class="grid">
  <div class="field">
    <label>Roll</label>
    <input type="number" id="ch_roll" min="1" max="16" value="1">
  </div>
  <div class="field">
    <label>Pitch</label>
    <input type="number" id="ch_pitch" min="1" max="16" value="2">
  </div>
  <div class="field">
    <label>Throttle</label>
    <input type="number" id="ch_throttle" min="1" max="16" value="3">
    <div class="note">Stick snaps to center (mid = 1500 &micro;s).</div>
  </div>
  <div class="field">
    <label>Yaw</label>
    <input type="number" id="ch_yaw" min="1" max="16" value="4">
  </div>
  <div class="field">
    <label>Arm switch</label>
    <input type="number" id="ch_arm" min="1" max="16" value="5">
    <div class="note">Low = disarmed &nbsp;/&nbsp; High = armed.</div>
  </div>
  <div class="field">
    <label>Flight mode (3-pos)</label>
    <input type="number" id="ch_fmode" min="1" max="16" value="6">
    <div class="note">STB=low &nbsp;ACRO=mid &nbsp;AUTO=high.</div>
  </div>
</div>

<div class="actions">
  <button id="save-btn" onclick="saveConfig()">Save &amp; Reboot</button>
  <button id="back-btn" onclick="location.href='/'">&#8592; Back to Controller</button>
</div>
<div id="msg"></div>

<script>
const fields = ['tx_pin','rx_pin','ch_roll','ch_pitch','ch_throttle','ch_yaw','ch_arm','ch_fmode','resolution'];

async function loadConfig() {
  try {
    const r = await fetch('/config/data');
    if (!r.ok) throw new Error(r.status);
    const d = await r.json();
    fields.forEach(k => {
      const el = document.getElementById(k);
      if (el && d[k] !== undefined) el.value = d[k];
    });
  } catch(e) {
    showMsg('Failed to load config: ' + e.message, false);
  }
}

async function saveConfig() {
  const body = {};
  for (const k of fields) {
    const v = parseInt(document.getElementById(k).value, 10);
    if (isNaN(v)) { showMsg('Invalid value for ' + k, false); return; }
    body[k] = v;
  }
  const btn = document.getElementById('save-btn');
  btn.disabled = true;
  btn.textContent = 'Saving\u2026';
  try {
    const r = await fetch('/config/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    });
    if (r.ok) {
      showMsg('Saved! Device is rebooting\u2026', true);
    } else {
      showMsg('Server error: ' + r.status, false);
      btn.disabled = false;
      btn.textContent = 'Save & Reboot';
    }
  } catch(e) {
    // Device will reboot so the request may fail — treat as success
    showMsg('Saved! Device is rebooting\u2026', true);
  }
}

function showMsg(text, ok) {
  const el = document.getElementById('msg');
  el.textContent = text;
  el.className   = ok ? 'ok' : 'err';
}

loadConfig();
</script>
</body>
</html>
)rawhtml";
