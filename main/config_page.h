#pragma once

// Minimal built-in settings page for the config server. Kept as a raw string
// so there is no filesystem dependency; it talks to the JSON API only.
static const char CONFIG_PAGE_HTML[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Obegränsad</title>
<style>
 body{font:14px/1.4 system-ui,sans-serif;margin:2rem;max-width:40rem}
 h1{font-size:1.2rem} section{margin:1rem 0;padding:1rem;border:1px solid #ccc;border-radius:8px}
 button{margin:.15rem} label{display:block;margin:.4rem 0}
 input{width:100%;padding:.3rem;box-sizing:border-box}
 pre{background:#f4f4f4;padding:.5rem;overflow:auto;font-size:12px}
</style>
</head>
<body>
<h1>Obegränsad</h1>
<section>
 <h2>State</h2>
 <pre id="state">loading…</pre>
 <div id="presets"></div>
 <div id="scenes"></div>
 <button onclick="control({action:'next_preset'})">Next preset</button>
 <button onclick="control({action:'prev_preset'})">Previous preset</button>
 <button onclick="control({action:'refresh_weather'})">Refresh weather</button>
 <button onclick="control({action:'redraw'})">Redraw</button>
</section>
<section>
 <h2>Configuration</h2>
 <form id="config">
  <label>Timezone <input name="timezone"></label>
  <label>Latitude <input name="latitude" type="number" step="0.0001"></label>
  <label>Longitude <input name="longitude" type="number" step="0.0001"></label>
  <label>Anniversary day <input name="anniversary_day" type="number" min="1" max="31"></label>
  <label>Anniversary month <input name="anniversary_month" type="number" min="1" max="12"></label>
  <label>Brightness (0-255) <input name="brightness" type="number" min="0" max="255"></label>
  <button type="submit">Save</button>
 </form>
</section>
<section>
 <h2>Firmware update</h2>
 <p>Upload a <code>build/obegransad.bin</code> produced by this repository.
    The device writes the inactive slot, verifies the image and restarts.</p>
 <input type="file" id="fw" accept=".bin,application/octet-stream">
 <button onclick="uploadFw()">Upload and restart</button>
 <progress id="fwprogress" value="0" max="100" style="width:100%"></progress>
 <div id="fwstatus"></div>
</section>
<script>
const $ = s => document.querySelector(s);
async function control(body){
  const r = await fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  if(!r.ok) alert(await r.text());
  refresh();
}
async function refresh(){
  const s = await (await fetch('/api/state')).json();
  $('#state').textContent = JSON.stringify(s,null,2);
  if(!$('#presets').dataset.done){
    const info = await (await fetch('/api/info')).json();
    $('#presets').innerHTML = info.presets.map(p=>`<button onclick="control({action:'preset',value:${p.index}})">${p.index}: ${p.name||'empty'}</button>`).join(' ');
    $('#scenes').innerHTML = info.scenes.map(n=>`<button onclick="control({action:'scene',value:${JSON.stringify(n)}})">${n}</button>`).join(' ');
    $('#presets').dataset.done = 1;
    const c = await (await fetch('/api/config')).json();
    for(const [k,v] of Object.entries(c)) if($(`[name=${k}]`)) $(`[name=${k}]`).value = v;
  }
}
function uploadFw(){
  const f = $('#fw').files[0];
  if(!f) return;
  const xhr = new XMLHttpRequest();
  xhr.open('POST','/api/ota');
  xhr.setRequestHeader('Content-Type','application/octet-stream');
  xhr.upload.onprogress = e => { if(e.lengthComputable) $('#fwprogress').value = 100*e.loaded/e.total; };
  xhr.onload = () => { $('#fwstatus').textContent = xhr.status + ' ' + xhr.responseText; };
  xhr.onerror = () => { $('#fwstatus').textContent = 'upload failed'; };
  xhr.send(f);
}
$('#config').addEventListener('submit', async e=>{
  e.preventDefault();
  const body = {};
  for(const el of e.target.elements) if(el.name) body[el.name] = el.type==='number' ? Number(el.value) : el.value;
  const r = await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  if(!r.ok) alert(await r.text());
  refresh();
});
refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)HTML";
