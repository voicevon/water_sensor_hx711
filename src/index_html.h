#pragma once

#include <Arduino.h>

// HX711 力传感器监控与配置中心 — SPA 暗色前端 HTML
// 存储于 Flash PROGMEM，不占用 RAM
static const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>HX711 Force Sensor Monitor</title>
<style>
:root{--bg:#0b0f19;--card:rgba(22,30,49,.85);--border:rgba(255,255,255,.08);--text:#f1f5f9;--muted:#64748b;--blue:#38bdf8;--cyan:#06b6d4;--green:#10b981;--red:#f43f5e;--yellow:#f59e0b;--nav:rgba(17,24,39,.97)}
*{box-sizing:border-box;margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
body{background:var(--bg);color:var(--text);min-height:100vh;display:flex;flex-direction:column;align-items:center}
header{width:100%;background:var(--nav);border-bottom:1px solid var(--border);padding:.85rem 1.5rem;position:sticky;top:0;z-index:100;backdrop-filter:blur(8px)}
.header-main{max-width:860px;margin:0 auto;display:flex;align-items:center;justify-content:space-between;width:100%}
.logo{font-size:1.15rem;font-weight:700;letter-spacing:.5px;background:linear-gradient(135deg,var(--blue),var(--cyan));-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.status-dots{display:flex;gap:.75rem;align-items:center;font-size:.8rem;color:var(--muted);background:rgba(0,0,0,.3);padding:.3rem .65rem;border-radius:20px;border:1px solid var(--border)}
.status-indicator{display:flex;align-items:center;gap:.35rem}
.dot{width:8px;height:8px;border-radius:50%;display:inline-block;background:#64748B;transition:all .3s ease}
.dot.on{background:var(--green);box-shadow:0 0 8px var(--green)}
.dot.off{background:var(--red);box-shadow:0 0 4px var(--red)}
.nav-tabs{display:flex;width:100%;background-color:var(--card);border:1px solid var(--border);border-radius:12px;padding:.3rem;margin-bottom:1.25rem;gap:.3rem}
.tab-item{flex:1;text-align:center;padding:.65rem .5rem;font-size:.88rem;font-weight:600;color:var(--muted);border-radius:8px;cursor:pointer;transition:all .2s ease;user-select:none}
.tab-item:hover{color:var(--text)}
.tab-item.active{background:linear-gradient(135deg,rgba(56,189,248,.18),rgba(6,182,212,.18));color:var(--blue);border:1px solid rgba(56,189,248,.35)}
.footer{text-align:center;font-size:.78rem;color:var(--muted);margin-top:1.5rem;border-top:1px solid var(--border);padding-top:1rem;line-height:1.8}
main{width:100%;max-width:860px;padding:1.5rem 1rem 3rem}
.tab-panel{display:none}.tab-panel.active{display:block}
.card{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:1.2rem 1.4rem;margin-bottom:1rem;backdrop-filter:blur(12px)}
.card-title{font-size:.78rem;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);margin-bottom:.8rem}
.sensor-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:1rem}
.sc{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:1.3rem;transition:all .25s;backdrop-filter:blur(12px)}
.sc.triggered{border-color:rgba(6,182,212,.5);box-shadow:0 0 18px rgba(6,182,212,.15)}
.sc-hdr{display:flex;align-items:center;justify-content:space-between;margin-bottom:1rem}
.sc-name{font-size:1rem;font-weight:600}
.badge{font-size:.7rem;padding:3px 10px;border-radius:20px;font-weight:600}
.badge.triggered{background:rgba(6,182,212,.18);color:var(--cyan)}
.badge.idle{background:rgba(100,116,139,.15);color:var(--muted)}
.metric{display:flex;justify-content:space-between;align-items:center;padding:.35rem 0;border-bottom:1px solid rgba(255,255,255,.04)}
.metric:last-child{border-bottom:none}
.metric label{font-size:.8rem;color:var(--muted)}
.metric span{font-size:.9rem;font-weight:600;font-variant-numeric:tabular-nums}
.force-val{font-size:1.6rem;font-weight:700;color:var(--blue);text-align:center;margin:.6rem 0}
.fu{font-size:.85rem;color:var(--muted);font-weight:400}
.btn-row{display:flex;gap:.5rem;margin-top:1rem;flex-wrap:wrap}
.btn{padding:.4rem .9rem;border-radius:8px;border:1px solid var(--border);cursor:pointer;font-size:.78rem;font-weight:600;transition:all .15s;background:rgba(255,255,255,.04);color:var(--text)}
.btn:hover{background:rgba(255,255,255,.1)}
.btn.primary{background:rgba(56,189,248,.15);color:var(--blue);border-color:rgba(56,189,248,.3)}
.btn.primary:hover{background:rgba(56,189,248,.25)}
.btn.danger{background:rgba(244,63,94,.1);color:var(--red);border-color:rgba(244,63,94,.25)}
.btn.danger:hover{background:rgba(244,63,94,.2)}
.btn.success{background:rgba(16,185,129,.12);color:var(--green);border-color:rgba(16,185,129,.3)}
.btn.success:hover{background:rgba(16,185,129,.22)}
.fg{margin-bottom:1rem}
.fg label{display:block;font-size:.8rem;color:var(--muted);margin-bottom:.4rem}
.fg input,.fg select{width:100%;padding:.55rem .8rem;background:rgba(255,255,255,.05);border:1px solid var(--border);border-radius:8px;color:var(--text);font-size:.9rem;outline:none;transition:border-color .2s}
.fg input:focus,.fg select:focus{border-color:var(--blue)}
.fg select option{background:#1e293b}
.form-row{display:grid;grid-template-columns:1fr 1fr;gap:1rem}
.sec{font-size:.9rem;font-weight:600;color:var(--blue);margin:1.2rem 0 .8rem;padding-bottom:.4rem;border-bottom:1px solid var(--border)}
#toast{position:fixed;bottom:1.5rem;right:1.5rem;padding:.7rem 1.2rem;background:rgba(22,30,49,.97);border:1px solid var(--border);border-radius:10px;font-size:.85rem;color:var(--text);transform:translateY(80px);opacity:0;transition:all .3s;pointer-events:none;z-index:999;max-width:280px}
#toast.show{transform:translateY(0);opacity:1}
#toast.ok{border-color:rgba(16,185,129,.4);color:var(--green)}
#toast.err{border-color:rgba(244,63,94,.4);color:var(--red)}
.wifi-list{max-height:200px;overflow-y:auto}
.wi{display:flex;justify-content:space-between;padding:.5rem .4rem;border-bottom:1px solid var(--border);cursor:pointer;border-radius:6px;transition:background .15s}
.wi:hover{background:rgba(255,255,255,.05)}
.wi span{font-size:.85rem}
.wi .rs{color:var(--muted);font-size:.78rem}
</style>
</head>
<body>
<header>
  <div class="header-main">
    <div class="logo">⚖ HX711 力传感器节点</div>
    <div class="status-dots">
      <span class="status-indicator"><span class="dot" id="dot-wifi"></span>WiFi</span>
      <span class="status-indicator"><span class="dot" id="dot-mqtt"></span>MQTT</span>
    </div>
  </div>
</header>
<main>
  <nav class="nav-tabs">
    <div class="tab-item active" onclick="showTab('monitor',this)">实时监控</div>
    <div class="tab-item" onclick="showTab('calibrate',this)">传感器标定</div>
    <div class="tab-item" onclick="showTab('network',this)">网络与系统</div>
  </nav>
  <div id="tab-monitor" class="tab-panel active">
    <div class="sensor-grid" id="sg"></div>
  </div>
  <div id="tab-calibrate" class="tab-panel">
    <div class="card">
      <div class="card-title">HX711 Channel Calibration</div>
      <div class="fg"><label>Channel</label>
        <select id="cal-ch">
          <option value="0">HX711 #1</option>
          <option value="1">HX711 #2</option>
          <option value="2">HX711 #3</option>
        </select>
      </div>
      <p class="sec">Tare (Zero)</p>
      <p style="font-size:.82rem;color:var(--muted);margin-bottom:.8rem">Remove all load, then click tare.</p>
      <button class="btn primary" onclick="doTare()">Tare (10 samples)</button>
      <p class="sec" style="margin-top:1.4rem">Scale Calibration</p>
      <p style="font-size:.82rem;color:var(--muted);margin-bottom:.8rem">Place known weight, then write scale value.</p>
      <div class="form-row">
        <div class="fg"><label>Known Weight (g)</label><input type="number" id="cal-known" placeholder="e.g. 500" min="1" step="0.1"></div>
        <div class="fg"><label>Scale Value</label><input type="number" id="cal-scale" placeholder="e.g. 2280.5" step="0.001"></div>
      </div>
      <button class="btn primary" onclick="doScale()">Write Scale</button>
      <p class="sec" style="margin-top:1.4rem">Channel Switch</p>
      <p style="font-size:.82rem;color:var(--muted);margin-bottom:.8rem">Changes take effect after reboot.</p>
      <div class="btn-row">
        <button class="btn success" onclick="doEnable()">Enable</button>
        <button class="btn danger"  onclick="doDisable()">Disable</button>
      </div>
    </div>
    <div class="card">
      <div class="card-title">Current Calibration Status</div>
      <div id="cal-status">Loading...</div>
    </div>
  </div>
  <div id="tab-network" class="tab-panel">
    <div class="card">
      <div class="card-title">WiFi and MQTT Configuration</div>
      <div class="fg"><label>STA WiFi SSID</label><input type="text" id="net-ssid" placeholder="WiFi name"></div>
      <div class="fg"><label>STA WiFi Password</label><input type="password" id="net-pass" placeholder="WiFi password"></div>
      <div class="fg"><label>Device Name (MQTT)</label><input type="text" id="net-name" placeholder="e.g. home"></div>
      <div class="form-row">
        <div class="fg"><label>MQTT Broker</label><input type="text" id="net-broker" placeholder="e.g. 192.168.1.100"></div>
        <div class="fg"><label>MQTT Port</label><input type="number" id="net-port" placeholder="1883" min="1" max="65535"></div>
      </div>
      <button class="btn primary" onclick="saveNetwork()">Save Config</button>
    </div>
    <div class="card">
      <div class="card-title">Nearby WiFi Networks</div>
      <button class="btn" onclick="scanWifi()" style="margin-bottom:.8rem">Scan</button>
      <div class="wifi-list" id="wifi-list"></div>
    </div>
  </div>
  <div class="footer">
    <p>版本: Version 2.0</p>
    <p>版权所有: 山东卷积分公司</p>
  </div>
</main>
<div id="toast"></div>
<script>
function showTab(id,btn){
  document.querySelectorAll('.tab-panel').forEach(p=>p.classList.remove('active'));
  document.querySelectorAll('.tab-item').forEach(b=>b.classList.remove('active'));
  document.getElementById('tab-'+id).classList.add('active');
  btn.classList.add('active');
  if(id==='calibrate')loadCalStatus();
  if(id==='network')loadNetwork();
}
let _tt;
function toast(msg,type='ok'){
  const el=document.getElementById('toast');
  el.textContent=msg;el.className='show '+type;
  clearTimeout(_tt);_tt=setTimeout(()=>el.className='',3000);
}
async function post(url,params){
  const r=await fetch(url,{method:'POST',body:new URLSearchParams(params)});
  const t=await r.text();if(!r.ok)throw new Error(t);return t;
}
function buildCards(sensors){
  const g=document.getElementById('sg');
  if(g.children.length!==sensors.length){
    g.innerHTML='';
    sensors.forEach((_,i)=>{
      g.insertAdjacentHTML('beforeend',
        '<div class="sc" id="sc'+i+'">'+
        '<div class="sc-hdr"><span class="sc-name">HX711 #'+(i+1)+'</span>'+
        '<span class="badge idle" id="bd'+i+'">IDLE</span></div>'+
        '<div class="force-val" id="fv'+i+'">--<span class="fu"> g</span></div>'+
        '<div class="metric"><label>Filtered (x0.1g)</label><span id="fi'+i+'">--</span></div>'+
        '<div class="metric"><label>Baseline</label><span id="ba'+i+'">--</span></div>'+
        '<div class="metric"><label>Threshold</label><span id="th'+i+'">--</span></div>'+
        '</div>');
    });
  }
  sensors.forEach((s,i)=>{
    const gram=(s.raw_val/10).toFixed(1);
    document.getElementById('fv'+i).innerHTML=gram+'<span class="fu"> g</span>';
    document.getElementById('fi'+i).textContent=s.filtered;
    document.getElementById('ba'+i).textContent=s.baseline;
    document.getElementById('th'+i).textContent=s.threshold;
    const sc=document.getElementById('sc'+i);
    const bd=document.getElementById('bd'+i);
    sc.className='sc'+(s.detected?' triggered':'');
    bd.className='badge'+(s.detected?' triggered':' idle');
    bd.textContent=s.detected?'TRIGGERED':'IDLE';
  });
}
async function updateData(){
  try{
    const r=await fetch('/api/data');const d=await r.json();
    document.getElementById('dot-wifi').className='dot '+(d.wifi_connected?'on':'off');
    document.getElementById('dot-mqtt').className='dot '+(d.mqtt_connected?'on':'off');
    buildCards(d.sensors);
  }catch(e){}
}
async function loadCalStatus(){
  try{
    const r=await fetch('/api/hx711');const d=await r.json();
    let h='';
    d.channels.forEach(c=>{
      const en=c.enabled?'<span style="color:var(--green)">Enabled</span>':'<span style="color:var(--red)">Disabled</span>';
      const ol=c.online?'<span style="color:var(--green)">Online</span>':'<span style="color:var(--muted)">Offline</span>';
      h+='<div style="padding:.6rem 0;border-bottom:1px solid var(--border)">'+
         '<strong>HX711 #'+(c.ch+1)+'</strong> &nbsp;'+en+' &nbsp;'+ol+'<br>'+
         '<span style="font-size:.82rem;color:var(--muted)">Scale: <b>'+c.scale.toFixed(4)+'</b> &nbsp;| Tare: <b>'+c.tare+'</b></span></div>';
    });
    document.getElementById('cal-status').innerHTML=h;
  }catch(e){document.getElementById('cal-status').textContent='Load failed';}
}
function calCh(){return document.getElementById('cal-ch').value;}
async function doTare(){
  try{const msg=await post('/api/hx711',{ch:calCh(),action:'tare'});toast('Tare OK: '+msg);loadCalStatus();}
  catch(e){toast('Tare failed: '+e.message,'err');}
}
async function doScale(){
  const scale=document.getElementById('cal-scale').value;
  if(!scale){toast('Enter scale value','err');return;}
  try{await post('/api/hx711',{ch:calCh(),action:'scale',scale});toast('Scale written');loadCalStatus();}
  catch(e){toast('Failed: '+e.message,'err');}
}
async function doEnable(){
  try{await post('/api/hx711',{ch:calCh(),action:'enable'});toast('Enabled (reboot to apply)');}
  catch(e){toast('Failed: '+e.message,'err');}
  loadCalStatus();
}
async function doDisable(){
  try{await post('/api/hx711',{ch:calCh(),action:'disable'});toast('Disabled (reboot to apply)','err');}
  catch(e){toast('Failed: '+e.message,'err');}
  loadCalStatus();
}
async function loadNetwork(){
  try{
    const r=await fetch('/api/sysconfig');const d=await r.json();
    document.getElementById('net-ssid').value=d.ssid||'';
    document.getElementById('net-pass').value=d.pass||'';
    document.getElementById('net-name').value=d.name||'';
    document.getElementById('net-broker').value=d.broker||'';
    document.getElementById('net-port').value=d.port||1883;
  }catch(e){}
}
async function saveNetwork(){
  try{
    await post('/api/sysconfig',{
      ssid:document.getElementById('net-ssid').value,
      password:document.getElementById('net-pass').value,
      name:document.getElementById('net-name').value,
      broker:document.getElementById('net-broker').value,
      port:document.getElementById('net-port').value
    });toast('Saved, reboot to apply');
  }catch(e){toast('Save failed: '+e.message,'err');}
}
async function scanWifi(){
  const list=document.getElementById('wifi-list');
  list.innerHTML='<span style="color:var(--muted);font-size:.82rem">Scanning...</span>';
  try{
    const r=await fetch('/api/scan');const d=await r.json();
    list.innerHTML='';
    d.networks.forEach(n=>{
      const div=document.createElement('div');div.className='wi';
      div.innerHTML='<span>'+n.ssid+'</span><span class="rs">'+n.rssi+' dBm</span>';
      div.onclick=()=>{document.getElementById('net-ssid').value=n.ssid;};
      list.appendChild(div);
    });
    if(!d.networks.length)list.innerHTML='<span style="color:var(--muted)">No networks found</span>';
  }catch(e){list.innerHTML='<span style="color:var(--red)">Scan failed</span>';}
}
updateData();
setInterval(updateData,1000);
</script>
</body>
</html>
)rawhtml";