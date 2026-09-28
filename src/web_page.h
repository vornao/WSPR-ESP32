// The web control page, served from flash. Polls /api/state once a second and
// posts changes to /api/cmd (with the X-Requested-With header web_ui.cpp requires).
#pragma once

static const char WEB_PAGE[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WSPR Beacon</title>
<style>
:root {
  --bg: #f4f5f7; --card: #fff; --text: #1b1d21; --muted: #6b7280; --line: #e3e5e9;
  --accent: #2563eb; --tx: #dc2626; --ok: #16a34a; --warn: #d97706;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #111317; --card: #1b1e24; --text: #e8eaed; --muted: #9aa0a8; --line: #2b2f37;
    --accent: #60a5fa; --tx: #f87171; --ok: #4ade80; --warn: #fbbf24;
  }
}
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--text);
  font: 15px/1.4 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
main { max-width: 560px; margin: 0 auto; padding: 16px; }
header { display: flex; align-items: center; justify-content: space-between; gap: 12px; margin: 4px 0 16px; }
h1 { font-size: 20px; margin: 0; }
.sub { color: var(--muted); font-size: 13px; }
.pill { padding: 4px 10px; border-radius: 999px; font-size: 13px; font-weight: 600;
  background: var(--line); color: var(--muted); white-space: nowrap; }
.pill.tx { background: var(--tx); color: #fff; }
.pill.carrier { background: var(--warn); color: #000; }
.pill.idle { background: var(--ok); color: #000; }
.card { background: var(--card); border: 1px solid var(--line); border-radius: 12px; padding: 16px; margin-bottom: 12px; }
.card h2 { font-size: 13px; text-transform: uppercase; letter-spacing: .05em; color: var(--muted); margin: 0 0 12px; }
.big { font-size: 22px; font-weight: 600; font-variant-numeric: tabular-nums; }
.bar { height: 8px; background: var(--line); border-radius: 4px; overflow: hidden; margin: 10px 0 4px; }
.bar > div { height: 100%; width: 0; background: var(--tx); transition: width .5s linear; }
.grid { display: grid; grid-template-columns: auto 1fr; gap: 6px 16px; margin-top: 12px; font-size: 14px; }
.grid span:nth-child(odd) { color: var(--muted); }
.num { font-variant-numeric: tabular-nums; }
.row { display: flex; align-items: center; gap: 8px; margin: 10px 0; flex-wrap: wrap; }
.row label { flex: 1 1 110px; color: var(--muted); }
input[type=number], select { flex: 1 1 140px; min-width: 0; padding: 8px 10px; font: inherit; color: var(--text);
  background: var(--bg); border: 1px solid var(--line); border-radius: 8px; }
button { padding: 8px 14px; font: inherit; font-weight: 600; border-radius: 8px; border: 1px solid var(--line);
  background: var(--bg); color: var(--text); cursor: pointer; }
button:hover { border-color: var(--accent); }
button.primary { background: var(--accent); border-color: var(--accent); color: #fff; }
button.danger { color: var(--tx); }
button.on { background: var(--accent); border-color: var(--accent); color: #fff; }
button:disabled { opacity: .45; cursor: default; }
.steps { display: grid; grid-template-columns: repeat(6, 1fr); gap: 6px; width: 100%; }
.steps button { padding: 8px 0; }
.seg { display: flex; gap: 6px; flex: 1 1 140px; }
.seg button { flex: 1; }
.switch { display: flex; align-items: center; justify-content: space-between; margin: 10px 0; }
.toggle { width: 48px; height: 28px; border-radius: 14px; padding: 0; position: relative; background: var(--line); border: 0; }
.toggle::after { content: ""; position: absolute; top: 3px; left: 3px; width: 22px; height: 22px; border-radius: 50%;
  background: #fff; transition: left .15s; }
.toggle.on { background: var(--accent); }
.toggle.on::after { left: 23px; }
.note { color: var(--muted); font-size: 13px; margin: 6px 0 0; }
table { width: 100%; border-collapse: collapse; font-size: 13px; font-variant-numeric: tabular-nums; }
th { text-align: left; color: var(--muted); font-weight: 600; padding: 4px 6px 6px 0; border-bottom: 1px solid var(--line); }
td { padding: 5px 6px 5px 0; border-bottom: 1px solid var(--line); }
tr:last-child td { border-bottom: 0; }
.scroll { max-height: 320px; overflow-y: auto; }
.stats { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; margin-bottom: 12px; }
.stat { background: var(--bg); border-radius: 8px; padding: 8px; }
.stat b { display: block; font-size: 18px; font-variant-numeric: tabular-nums; }
.stat span { color: var(--muted); font-size: 12px; }
.st-done { color: var(--ok); } .st-aborted { color: var(--warn); } .st-onair { color: var(--tx); font-weight: 600; }
#toast { position: fixed; left: 50%; bottom: 20px; transform: translateX(-50%); max-width: calc(100% - 32px);
  padding: 10px 16px; border-radius: 8px; font-weight: 600; text-align: center; opacity: 0; transition: opacity .2s;
  pointer-events: none; color: #fff; background: var(--tx); }
#toast.ok { background: var(--ok); color: #000; }
#toast.show { opacity: 1; }
/* Desktop / tablet landscape: controls on the left, spots and log on the right.
   The min-height keeps phones in landscape on the single-column layout. */
@media (min-width: 900px) and (min-height: 500px) {
  main { max-width: 1320px; padding: 0 24px 24px; }
  header { position: sticky; top: 0; z-index: 1; background: var(--bg); margin: 0 0 4px; padding: 16px 0 12px; }
  h1 { font-size: 22px; }
  .layout { display: grid; grid-template-columns: minmax(360px, 420px) minmax(0, 1fr); gap: 16px; align-items: start; }
  .layout > div { min-width: 0; }
  .card { margin-bottom: 16px; }
  .row label { flex: 0 0 120px; }
  .seg button { white-space: nowrap; }
  #spotsScroll { max-height: max(320px, calc(100vh - 380px)); }
  #historyScroll { max-height: max(240px, 40vh); }
  th { position: sticky; top: 0; background: var(--card); }
}
</style>
</head>
<body>
<main>
  <header>
    <div>
      <h1 id="station">WSPR beacon</h1>
      <div class="sub" id="clock">--</div>
    </div>
    <span class="pill" id="pill">connecting</span>
  </header>

  <div class="layout">
  <div>
  <section class="card">
    <h2>Status</h2>
    <div class="big" id="headline">--</div>
    <div class="bar" id="barWrap" hidden><div id="bar"></div></div>
    <div class="sub" id="detail"></div>
    <div class="grid">
      <span>Schedule</span><span id="schedule">--</span>
      <span>Last TX</span><span class="num" id="lastTx">--</span>
      <span>Centre</span><span class="num" id="center">--</span>
      <span>Effective</span><span class="num" id="effective">--</span>
      <span>WiFi</span><span id="wifi">--</span>
      <span>Time</span><span id="ntp">--</span>
      <span>Si5351</span><span id="chip">--</span>
    </div>
  </section>

  <section class="card">
    <h2>Beacon</h2>
    <div class="switch"><span>Scheduled transmissions</span><button class="toggle" id="beacon" aria-label="Beacon on/off"></button></div>
    <div class="row">
      <label for="interval">Transmit every</label>
      <select id="interval"></select>
    </div>
    <div class="row">
      <label>Message</label>
      <div class="seg" id="msgmode">
        <button data-mode="1">Type 1</button><button data-mode="0">Alternate</button><button data-mode="2">Type 3</button>
      </div>
    </div>
    <p class="note" id="msgNote"></p>
    <div class="row">
      <button class="primary" id="next">Transmit next slot</button>
      <button class="danger" id="cancel">Cancel request</button>
    </div>
  </section>

  <section class="card">
    <h2>Radio</h2>
    <div class="row">
      <label for="freq">Centre (Hz)</label>
      <input type="number" id="freq" step="1">
      <button id="setFreq">Set</button>
    </div>
    <div class="steps">
      <button data-step="-100">-100</button><button data-step="-10">-10</button><button data-step="-1">-1</button>
      <button data-step="1">+1</button><button data-step="10">+10</button><button data-step="100">+100</button>
    </div>
    <div class="row">
      <label for="corr">Correction (ppb)</label>
      <input type="number" id="corr" step="1">
      <button id="setCorr">Set</button>
    </div>
    <div class="row">
      <label>Drive</label>
      <div class="seg" id="drive">
        <button data-ma="2">2 mA</button><button data-ma="4">4 mA</button><button data-ma="6">6 mA</button><button data-ma="8">8 mA</button>
      </div>
    </div>
    <div class="switch"><span>Test carrier (pauses beacon)</span><button class="toggle" id="carrier" aria-label="Test carrier on/off"></button></div>
    <div class="switch"><span>Tone sweep: the 4 WSPR tones in turn</span><button class="toggle" id="tones" aria-label="Tone sweep on/off"></button></div>
    <p class="note">Frequency, correction and the test outputs are locked while transmitting.</p>
  </section>
  </div>

  <div>
  <section class="card">
    <h2>Heard by</h2>
    <div class="row">
      <label for="spotWin">Last</label>
      <select id="spotWin">
        <option value="1">1 hour</option><option value="6">6 hours</option>
        <option value="24" selected>24 hours</option><option value="168">7 days</option>
      </select>
      <button id="spotRefresh">Refresh</button>
    </div>
    <div class="stats">
      <div class="stat"><b id="stSpots">-</b><span>spots</span></div>
      <div class="stat"><b id="stRx">-</b><span>receivers</span></div>
      <div class="stat"><b id="stDx">-</b><span>best km</span></div>
      <div class="stat"><b id="stSnr">-</b><span>best SNR</span></div>
    </div>
    <div class="scroll" id="spotsScroll"><table>
      <thead><tr><th>UTC</th><th>Receiver</th><th>Loc</th><th>km</th><th>SNR</th><th>Drift</th></tr></thead>
      <tbody id="spots"></tbody>
    </table></div>
    <p class="note" id="spotNote">Spots from wspr.live (reported to wsprnet.org; a few minutes' delay).</p>
  </section>

  <section class="card">
    <h2>Transmissions</h2>
    <div class="scroll" id="historyScroll"><table>
      <thead><tr><th>UTC</th><th>Frequency</th><th>Message</th><th>Status</th></tr></thead>
      <tbody id="history"></tbody>
    </table></div>
    <p class="note" id="historyNote">Kept in memory since the last restart (up to 20).</p>
  </section>

  <section class="card">
    <h2>System</h2>
    <p class="note" style="margin-top:0">Changes to frequency, correction, drive, interval, message and beacon on/off are saved and survive restarts.
      Firmware updates over WiFi: <code>pio run -e esp32c3-ota -t upload</code></p>
    <div class="row"><button class="danger" id="reset">Restore defaults (config.h)</button></div>
  </section>
  </div>
  </div>
</main>
<div id="toast"></div>

<script>
const $ = id => document.getElementById(id);
let s = null;          // last state from the device
let clockBase = null;  // {utcMs, at: performance.now()} for a smooth local clock
let seq = 0;           // bumped on every state update, so late poll replies are dropped

for (let n = 1; n <= 30; n++) {
  const o = document.createElement('option');
  o.value = n;
  o.textContent = n === 1 ? 'slot (every 2 min)' : `${n} slots (every ${n * 2} min)`;
  $('interval').appendChild(o);
}

const fmtHz = hz => Number(hz).toLocaleString('en-US').replace(/,/g, ' ') + ' Hz';
const pad = n => String(n).padStart(2, '0');
const fmtUtc = ms => { const d = new Date(ms); return `${pad(d.getUTCHours())}:${pad(d.getUTCMinutes())}:${pad(d.getUTCSeconds())}`; };
const fmtDur = sec => sec < 60 ? `${sec}s` : `${Math.floor(sec / 60)}m ${pad(sec % 60)}s`;

function toast(msg, ok) {
  const t = $('toast');
  t.textContent = msg;
  t.className = 'show' + (ok ? ' ok' : '');
  clearTimeout(t._h);
  t._h = setTimeout(() => t.className = ok ? 'ok' : '', 3000);
}

function applyState(state) {
  seq++;
  s = state;
  clockBase = s.utcMs > 0 ? { utcMs: s.utcMs, at: performance.now() } : null;
  render();
}

const msgName = (st, part) => (part === 1 ? st.msg2 : st.msg1) + ' dBm';
const MODE_NOTES = {
  0: 'Alternates Type 1 (call + 4-char locator) and Type 3 (full 6-char locator). Receivers show the Type 3 only after decoding a Type 1.',
  1: 'Type 1 only: callsign, 4-char locator, power. Every station can decode it; best for weak signals.',
  2: 'Type 3 only: hashed callsign + 6-char locator. Receivers that never decoded your Type 1 show it as <...>.',
};

const nextTxText = st => st.nextTx > 0 ? `next TX ${fmtUtc(st.nextTx * 1000)} UTC`
                                       : (st.synced ? 'no TX scheduled' : 'waiting for NTP');

// Confirmation shown after a successful command, built from the device's new state.
function feedback(name, before, after) {
  switch (name) {
    case 'interval': return `Every ${after.everyN * 2} min · ${nextTxText(after)}`;
    case 'beacon':   return after.beacon ? `Beacon on · ${nextTxText(after)}` : 'Beacon off';
    case 'next':     return `TX queued · ${nextTxText(after)}`;
    case 'cancel':   return before.transmitting ? 'Transmission aborted'
                          : before.pending ? 'TX request cleared' : 'Nothing to cancel';
    case 'freq':     return `Centre ${fmtHz(after.centerHz)}`;
    case 'correction': return `Correction ${after.correctionPpb} ppb`;
    case 'drive':    return `Drive ${after.driveMa} mA`;
    case 'carrier':  return after.carrier ? 'Test carrier on' : 'Test carrier off';
    case 'tones':    return after.tones ? 'Tone sweep on' : 'Tone sweep off';
    case 'msgmode':  return `Next TX: ${msgName(after, after.nextPart)}`;
    case 'reset':    return 'Settings restored to config.h defaults';
  }
  return 'Done';
}

async function cmd(name, value) {
  const before = s;
  const body = new URLSearchParams({ cmd: name });
  if (value !== undefined) body.set('value', value);
  try {
    const r = await fetch('/api/cmd', { method: 'POST', body, headers: { 'X-Requested-With': 'wspr' } });
    const j = await r.json();
    if (j.state) applyState(j.state);
    if (j.ok) toast(feedback(name, before || j.state, j.state), true);
    else toast(j.error || 'failed', false);
  } catch (e) {
    toast('device unreachable', false);
    refresh();
  }
}

function nowUtcMs() {
  return clockBase ? clockBase.utcMs + (performance.now() - clockBase.at) : null;
}

function render() {
  if (!s) return;
  const now = nowUtcMs();
  $('station').textContent = `${s.call} · ${s.locator} · ${s.dbm} dBm`;
  $('clock').textContent = now ? fmtUtc(now) + ' UTC' : 'waiting for NTP';

  const pill = $('pill');
  if (s.transmitting) { pill.textContent = 'TX'; pill.className = 'pill tx'; }
  else if (s.carrier) { pill.textContent = 'CARRIER'; pill.className = 'pill carrier'; }
  else if (s.tones) { pill.textContent = 'TONES'; pill.className = 'pill carrier'; }
  else if (!s.synced) { pill.textContent = 'NO TIME'; pill.className = 'pill'; }
  else if (s.beacon) { pill.textContent = 'READY'; pill.className = 'pill idle'; }
  else { pill.textContent = 'OFF'; pill.className = 'pill'; }

  $('barWrap').hidden = !s.transmitting;
  if (s.transmitting) {
    $('headline').textContent = `Transmitting ${fmtHz(s.txFreqHz)}`;
    $('bar').style.width = (100 * s.symbol / s.symbols) + '%';
    $('detail').textContent = `Symbol ${s.symbol} / ${s.symbols} · ${msgName(s, s.part)}`;
  } else if (s.carrier) {
    $('headline').textContent = `Carrier on ${fmtHz(s.centerHz)}`;
    $('detail').textContent = 'Steady test carrier, beacon paused';
  } else if (s.tones) {
    $('headline').textContent = `Tone ${s.tone} on air`;
    $('detail').textContent = `Tone sweep above ${fmtHz(s.centerHz)}, beacon paused`;
  } else if (s.nextTx > 0 && now) {
    const sec = Math.max(0, Math.round(s.nextTx - now / 1000));
    $('headline').textContent = `Next TX in ${fmtDur(sec)}`;
    $('detail').textContent = `at ${fmtUtc(s.nextTx * 1000)} UTC, centre ±${s.randomOffsetHz} Hz · ${msgName(s, s.nextPart)}` +
                              (s.pending ? ' · requested manually' : '');
  } else {
    $('headline').textContent = s.synced ? 'Idle' : 'Waiting for time sync';
    $('detail').textContent = s.messageOk ? '' : 'Message invalid: check callsign / locator in config.h';
  }

  $('schedule').textContent = s.beacon ? `every ${s.everyN} slot${s.everyN > 1 ? 's' : ''} (${s.everyN * 2} min)` : 'off';
  $('lastTx').textContent = s.lastTx > 0 ? fmtUtc(s.lastTx * 1000) + ' UTC' : 'none yet';
  $('center').textContent = fmtHz(s.centerHz);
  $('effective').textContent = `${fmtHz(s.effectiveHz.toFixed(2))} (nominal crystal)`;
  $('wifi').textContent = s.wifi ? `connected, ${s.rssi} dBm` : 'disconnected';
  $('ntp').textContent = s.synced ? 'NTP synced' : 'not synced';
  $('chip').textContent = s.radioOk ? 'OK' : 'NOT RESPONDING';

  $('beacon').classList.toggle('on', s.beacon);
  $('carrier').classList.toggle('on', s.carrier);
  $('tones').classList.toggle('on', s.tones);
  if (document.activeElement !== $('interval')) $('interval').value = s.everyN;
  if (document.activeElement !== $('freq')) $('freq').value = s.centerHz;
  if (document.activeElement !== $('corr')) $('corr').value = s.correctionPpb;
  document.querySelectorAll('#drive button').forEach(b => b.classList.toggle('on', +b.dataset.ma === s.driveMa));
  document.querySelectorAll('#msgmode button').forEach(b => {
    b.classList.toggle('on', +b.dataset.mode === (s.parts < 2 ? 1 : s.msgMode));
    b.disabled = s.parts < 2 && b.dataset.mode !== '1';
  });
  $('msgNote').textContent = s.parts < 2 ? 'Set a 6-char locator in config.h to enable Type 3.' : MODE_NOTES[s.msgMode];

  const locked = s.transmitting;
  ['setFreq', 'setCorr', 'carrier', 'tones'].forEach(id => $(id).disabled = locked);
  document.querySelectorAll('.steps button').forEach(b => b.disabled = locked);
  $('cancel').disabled = !(s.transmitting || s.pending);
  $('cancel').textContent = s.transmitting ? 'Abort TX' : 'Cancel request';
  $('next').disabled = s.pending || s.transmitting;
}

async function refresh() {
  const mySeq = seq;
  try {
    const r = await fetch('/api/state', { cache: 'no-store' });
    const state = await r.json();
    if (mySeq === seq) applyState(state);  // a command reply arrived meanwhile: it is newer
  } catch (e) {
    $('pill').textContent = 'OFFLINE';
    $('pill').className = 'pill';
  }
}

$('beacon').onclick = () => cmd('beacon', s && s.beacon ? 0 : 1);
$('carrier').onclick = () => cmd('carrier', s && s.carrier ? 0 : 1);
$('tones').onclick = () => cmd('tones', s && s.tones ? 0 : 1);
$('interval').onchange = e => cmd('interval', e.target.value);
$('next').onclick = () => cmd('next');
$('cancel').onclick = () => cmd('cancel');
$('setFreq').onclick = () => cmd('freq', $('freq').value);
$('setCorr').onclick = () => cmd('correction', $('corr').value);
$('freq').onkeydown = e => { if (e.key === 'Enter') $('setFreq').click(); };
$('corr').onkeydown = e => { if (e.key === 'Enter') $('setCorr').click(); };
document.querySelectorAll('.steps button').forEach(b =>
  b.onclick = () => s && cmd('freq', s.centerHz + +b.dataset.step));
document.querySelectorAll('#drive button').forEach(b => b.onclick = () => cmd('drive', b.dataset.ma));
document.querySelectorAll('#msgmode button').forEach(b => b.onclick = () => cmd('msgmode', b.dataset.mode));
$('reset').onclick = () => { if (confirm('Restore the config.h defaults and erase the saved settings?')) cmd('reset'); };

// ---------- transmission log (from the device) ----------
let historyKey = null;
function cell(tr, text, cls) { const td = document.createElement('td'); td.textContent = text; if (cls) td.className = cls; tr.appendChild(td); }
async function loadHistory() {
  try {
    const r = await fetch('/api/history', { cache: 'no-store' });
    const list = await r.json();
    const body = $('history');
    body.replaceChildren();
    for (const h of list) {
      const tr = document.createElement('tr');
      cell(tr, fmtUtc(h.t * 1000));
      cell(tr, fmtHz(h.f));
      cell(tr, h.msg);
      cell(tr, { done: 'done', aborted: 'aborted', onair: 'on air' }[h.st] || h.st, 'st-' + h.st);
      body.appendChild(tr);
    }
    $('historyNote').textContent = list.length ? 'Kept in memory since the last restart (up to 20).'
                                               : 'No transmissions since the last restart.';
  } catch (e) { /* device offline: the status pill already shows it */ }
}
// Reload the log when a transmission starts or ends.
setInterval(() => {
  if (!s) return;
  const key = `${s.lastTx}|${s.transmitting}`;
  if (key !== historyKey) { historyKey = key; loadHistory(); }
}, 1000);

// ---------- spots (straight from wspr.live, by the browser) ----------
let spotTimer = null;
async function loadSpots() {
  if (!s) { setTimeout(loadSpots, 1000); return; }
  const call = String(s.call).toUpperCase();
  if (!/^[A-Z0-9\/]+$/.test(call)) return;
  const hours = +$('spotWin').value;
  const q = `SELECT time, rx_sign, rx_loc, distance, snr, drift FROM wspr.rx ` +
            `WHERE tx_sign = '${call}' AND time > now() - INTERVAL ${hours} HOUR ` +
            `ORDER BY time DESC LIMIT 200 FORMAT JSON`;
  $('spotNote').textContent = 'Loading spots from wspr.live...';
  try {
    const r = await fetch('https://db1.wspr.live/?query=' + encodeURIComponent(q));
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const rows = (await r.json()).data || [];
    const body = $('spots');
    body.replaceChildren();
    for (const x of rows) {
      const tr = document.createElement('tr');
      cell(tr, String(x.time).slice(11, 16));
      cell(tr, x.rx_sign);
      cell(tr, x.rx_loc);
      cell(tr, Number(x.distance).toLocaleString('en-US'));
      cell(tr, `${x.snr} dB`);
      cell(tr, x.drift);
      body.appendChild(tr);
    }
    const rx = new Set(rows.map(x => x.rx_sign));
    $('stSpots').textContent = rows.length;
    $('stRx').textContent = rx.size;
    $('stDx').textContent = rows.length ? Math.max(...rows.map(x => +x.distance)).toLocaleString('en-US') : '-';
    $('stSnr').textContent = rows.length ? Math.max(...rows.map(x => +x.snr)) + ' dB' : '-';
    $('spotNote').textContent = rows.length
      ? `Spots of ${call} from wspr.live, updated ${fmtUtc(Date.now())} UTC (a few minutes' delay).`
      : `No spots of ${call} in the last ${hours} h yet (wspr.live updates with a few minutes' delay).`;
  } catch (e) {
    $('spotNote').textContent = 'Could not reach wspr.live (needs internet on this device/browser).';
  }
  clearTimeout(spotTimer);
  spotTimer = setTimeout(loadSpots, 5 * 60 * 1000);  // wspr.live allows ~20 queries/min; be gentle
}
$('spotRefresh').onclick = loadSpots;
$('spotWin').onchange = loadSpots;
loadSpots();

refresh();
setInterval(refresh, 1000);
setInterval(render, 250);
</script>
</body>
</html>
)HTML";
