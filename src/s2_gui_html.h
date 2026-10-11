#pragma once

static const char * s2_gui_html = R"S2GUI(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>S2 Studio</title>
<style>
:root { color-scheme: dark; --bg:#0d1117; --panel:#171d27; --panel2:#1d2531; --line:#303b4a; --text:#f3f5f8; --muted:#a8b4c5; --accent:#78e2c1; --blue:#9ab9ff; --warn:#ffcf91; font-family:Inter,ui-sans-serif,system-ui,-apple-system,Segoe UI,sans-serif }
* { box-sizing:border-box } body { margin:0; background:radial-gradient(circle at 85% 0%,#1b3440 0,transparent 35%),var(--bg); color:var(--text); min-height:100vh }
header { border-bottom:1px solid var(--line); background:#101720cc; backdrop-filter:blur(16px) } .top { max-width:1160px; margin:auto; padding:17px 24px; display:flex; align-items:center; justify-content:space-between; gap:20px }
.brand { font-weight:800; letter-spacing:-.04em; font-size:21px }.brand span { color:var(--accent) }.tag { border:1px solid var(--line); color:var(--muted); padding:6px 11px; border-radius:100px; font-size:12px }
main { max-width:1160px; margin:auto; padding:43px 24px 70px } .hero { max-width:770px; margin-bottom:32px } .eyebrow { font-size:12px; text-transform:uppercase; letter-spacing:.16em; color:var(--accent); font-weight:800 } h1 { font-size:clamp(34px,5vw,56px); letter-spacing:-.055em; line-height:1.05; margin:12px 0 14px } p { color:var(--muted); line-height:1.55; margin:0 } .layout { display:grid; grid-template-columns:1.1fr .9fr; gap:18px }
.card { border:1px solid var(--line); background:linear-gradient(145deg,#1b2330,#141a23); border-radius:18px; padding:25px; box-shadow:0 14px 35px #0002 } .card h2 { margin:0 0 5px; font-size:20px; letter-spacing:-.025em }.hint { font-size:13px; margin-bottom:23px } .stack { display:grid; gap:18px }.row { display:flex; justify-content:space-between; align-items:center; gap:12px; margin-bottom:7px }
label { display:block; font-size:13px; font-weight:700; color:#d9e2ef; margin-bottom:7px } small { color:var(--muted); font-size:12px } input,textarea,select { width:100%; color:var(--text); background:#101720; border:1px solid var(--line); border-radius:10px; padding:12px 13px; font:inherit; outline:none } input:focus,textarea:focus,select:focus { border-color:var(--accent); box-shadow:0 0 0 3px #78e2c126 } textarea { min-height:120px; resize:vertical } input[type=file] { padding:10px; font-size:13px } input[type=range] { padding:0; accent-color:var(--accent) }
.tabs { display:flex; gap:7px; background:#101720; padding:5px; border-radius:12px; margin:22px 0 }.tabs button { flex:1; border:0; background:transparent; color:var(--muted); padding:10px 6px; border-radius:8px; font-weight:700; cursor:pointer }.tabs button[aria-selected=true] { background:var(--panel2); color:var(--text) }.mode { display:none }.mode.active { display:grid; gap:16px } .two { display:grid; grid-template-columns:1fr 1fr; gap:13px }
button.primary { border:0; background:var(--accent); color:#0b2420; border-radius:10px; font:inherit; font-weight:800; padding:13px 18px; cursor:pointer; width:100%; margin-top:20px } button.primary:hover { background:#a1f0d7 } button:disabled { opacity:.55; cursor:wait } .secondary { border:1px solid var(--line); background:#222c39; color:var(--text); padding:9px 12px; border-radius:8px; cursor:pointer } .notice { border:1px solid #755d39; background:#3c3022; color:#ffe3b7; padding:14px; border-radius:10px; font-size:13px; line-height:1.5 } .status { margin-top:15px; min-height:20px; font-size:13px; color:var(--muted) }.status.error { color:#ff9e9e }.result { margin-top:20px; border-top:1px solid var(--line); padding-top:19px; display:none }.result audio { width:100%; margin:10px 0 14px }.download { display:inline-block; color:var(--accent); font-weight:700; text-decoration:none; font-size:13px }.subhead { color:var(--blue); font-size:12px; font-weight:800; text-transform:uppercase; letter-spacing:.12em; margin:3px 0 13px } .mutedlink { color:var(--blue); text-decoration:none } footer { margin-top:27px; color:var(--muted); font-size:12px }
@media(max-width:800px) { .layout { grid-template-columns:1fr } main { padding-top:32px } .card { padding:20px } } @media(max-width:430px) { .two { grid-template-columns:1fr } .tag { display:none } }
</style>
</head>
<body>
<header><div class="top"><div class="brand">s2<span>.</span>studio</div><div class="tag">Local voice workspace</div></div></header>
<main>
<div class="hero"><div class="eyebrow">Voice cloning workspace</div><h1>Give your words a voice.</h1><p>Create speech, try a reference recording, and save voices for later. Processing runs on the S2 server you started.</p></div>
<div class="layout"><section class="card"><h2>Generate speech</h2><p class="hint">Enter a line, choose a voice source, then listen or download.</p>
<form id="generate-form"><label for="text">Text to speak</label><textarea id="text" required placeholder="Write what you want the voice to say..."></textarea>
<div class="tabs" role="tablist" aria-label="Voice source"><button type="button" role="tab" data-mode="default" aria-selected="true">Default</button><button type="button" role="tab" data-mode="clone" aria-selected="false">Reference clip</button><button type="button" role="tab" data-mode="saved" aria-selected="false">Saved voice</button></div>
<div class="mode active" id="mode-default"><p class="hint">Use the model's default voice.</p></div>
<div class="mode" id="mode-clone"><div><label for="clone-audio">Reference audio · WAV or MP3</label><input id="clone-audio" type="file" accept="audio/wav,audio/mpeg,.wav,.mp3"><small>A clean 5–30 second clip usually works best.</small></div><div><label for="clone-transcript">Exact words spoken in the clip</label><textarea id="clone-transcript" placeholder="Transcript of your reference recording"></textarea></div></div>
<div class="mode" id="mode-saved"><label for="saved-voice">Saved voice</label><select id="saved-voice"><option value="">Choose a voice</option></select><small>Profiles made here are stored in the server's voice directory.</small></div>
<div class="row" style="margin-top:20px"><label for="temperature" style="margin:0">Variation</label><small id="temperature-value">0.8</small></div><input id="temperature" type="range" min="0.1" max="1.5" value="0.8" step="0.1"><small>Lower values are steadier; higher values may sound more varied.</small>
<button class="primary" id="generate-button">Generate audio</button><div class="status" id="generate-status" role="status" aria-live="polite"></div></form>
<div class="result" id="result"><div class="subhead">Latest result</div><audio id="player" controls></audio><a id="download" class="download" download="s2-generated.wav">Download WAV ↓</a></div></section>
<aside class="stack"><section class="card"><h2>Save a voice</h2><p class="hint">Encode a reference recording once and reuse it in the Saved voice tab. This creates a voice profile; it does not train model weights.</p><form id="save-form"><div><label for="voice-name">Voice name</label><input id="voice-name" required maxlength="64" pattern="[A-Za-z0-9_-]+" placeholder="e.g. narrator_01"><small>Letters, numbers, underscores, and dashes.</small></div><div style="margin-top:17px"><label for="save-audio">Reference audio · WAV or MP3</label><input id="save-audio" required type="file" accept="audio/wav,audio/mpeg,.wav,.mp3"></div><div style="margin-top:17px"><label for="save-transcript">Exact words spoken</label><textarea id="save-transcript" required placeholder="Transcript of the recording"></textarea></div><button class="primary" id="save-button">Save voice profile</button><div class="status" id="save-status" role="status" aria-live="polite"></div></form></section>
<section class="card"><div class="subhead">Voice design &amp; training</div><div class="notice">S2.cpp currently supports reference based cloning, not text described voice design or fine tuning. To shape a voice, try different reference clips and save the one you like. Variation changes sampling, not the speaker identity.</div></section></aside></div>
<footer>For access from another device on your LAN, start with <code>--host &lt;server-lan-ip&gt;</code> and open that address and port in your browser.</footer>
</main>
<script>
const $ = id => document.getElementById(id);
let mode = 'default', audioUrl = null;
document.querySelectorAll('[data-mode]').forEach(tab => tab.addEventListener('click', () => {
  mode = tab.dataset.mode;
  document.querySelectorAll('[data-mode]').forEach(button => button.setAttribute('aria-selected', String(button === tab)));
  document.querySelectorAll('.mode').forEach(panel => panel.classList.toggle('active', panel.id === 'mode-' + mode));
}));
$('temperature').addEventListener('input', () => $('temperature-value').textContent = $('temperature').value);
async function voices(select) {
  try {
    const response = await fetch('/voices');
    if (!response.ok) throw new Error('Could not load saved voices.');
    const data = await response.json();
    $('saved-voice').replaceChildren(new Option('Choose a voice', ''));
    data.voices.forEach(id => $('saved-voice').add(new Option(id, id)));
    if (select) $('saved-voice').value = select;
  } catch (error) { $('save-status').textContent = error.message; }
}
async function errorMessage(response) {
  const body = await response.text();
  try { return JSON.parse(body).error || body; } catch { return body || `Request failed (${response.status})`; }
}
function status(id, message, failed = false) { $(id).textContent = message; $(id).classList.toggle('error', failed); }
$('generate-form').addEventListener('submit', async event => {
  event.preventDefault();
  const data = new FormData();
  data.set('text', $('text').value.trim());
  if (!data.get('text')) return status('generate-status', 'Enter some text first.', true);
  if (mode === 'clone') {
    if (!$('clone-audio').files[0] || !$('clone-transcript').value.trim()) return status('generate-status', 'Add a reference clip and its exact transcript.', true);
    data.set('reference', $('clone-audio').files[0]);
    data.set('reference_text', $('clone-transcript').value.trim());
  } else if (mode === 'saved') {
    if (!$('saved-voice').value) return status('generate-status', 'Choose a saved voice.', true);
    data.set('voice', $('saved-voice').value);
  }
  data.set('params', JSON.stringify({temperature:Number($('temperature').value)}));
  $('generate-button').disabled = true;
  status('generate-status', 'Generating audio… this may take a while.');
  try {
    const response = await fetch('/generate', {method:'POST',body:data});
    if (!response.ok) throw new Error(await errorMessage(response));
    const blob = await response.blob();
    if (!blob.size) throw new Error('Server returned an empty audio file.');
    if (audioUrl) URL.revokeObjectURL(audioUrl);
    audioUrl = URL.createObjectURL(blob);
    $('player').src = audioUrl;
    $('download').href = audioUrl;
    $('result').style.display = 'block';
    status('generate-status', 'Audio ready.');
  } catch (error) { status('generate-status', error.message, true); }
  finally { $('generate-button').disabled = false; }
});
$('save-form').addEventListener('submit', async event => {
  event.preventDefault();
  const data = new FormData();
  data.set('name', $('voice-name').value.trim());
  data.set('transcript', $('save-transcript').value.trim());
  data.set('reference', $('save-audio').files[0]);
  $('save-button').disabled = true;
  status('save-status', 'Encoding reference audio…');
  try {
    const response = await fetch('/voices', {method:'POST',body:data});
    if (!response.ok) throw new Error(await errorMessage(response));
    const result = await response.json();
    await voices(result.voice);
    status('save-status', `Saved “${result.voice}”. Select it in Saved voice to test.`);
  } catch (error) { status('save-status', error.message, true); }
  finally { $('save-button').disabled = false; }
});
voices();
</script>
</body></html>)S2GUI";
