#pragma once

// Static settings page (flash only; the values come from /api/settings).
static const char SETTINGS_PAGE[] PROGMEM = R"HTML(<!doctype html><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Bot settings</title>
<style>
body{margin:0;background:#0f1514;color:#d6e2de;font:15px/1.45 system-ui,sans-serif}
.w{max-width:520px;margin:0 auto;padding:16px 16px 48px}
h1{font-size:1.25rem;margin:.4rem 0 .2rem}
h2{font-size:.8rem;letter-spacing:.08em;text-transform:uppercase;color:#7d8a85;margin:1.6rem 0 .4rem}
a{color:#3fbaa3}
.card{background:#162120;border:1px solid #263230;border-radius:10px;padding:4px 14px}
.f{padding:10px 0;border-bottom:1px solid #263230}.f:last-child{border-bottom:0}
label{display:block;color:#9fb0aa;font-size:.85rem}
input[type=text],input[type=number],select{width:100%;box-sizing:border-box;margin-top:4px;padding:10px;background:#0f1514;color:inherit;border:1px solid #2d3d3a;border-radius:8px;font-size:1rem}
.h{color:#7d8a85;font-size:.8rem;margin-top:3px}
.sw{display:flex;justify-content:space-between;align-items:center;gap:12px}
.sw input{width:22px;height:22px;accent-color:#3fbaa3}
.sl{border-top:1px solid #263230;padding:10px 0}.sl:first-child{border-top:0}
.stt{font-size:.85rem;margin-top:6px;color:#7d8a85}.stt.ok{color:#3fbaa3}.stt.bad{color:#e07a7a}
.cmds{display:grid;grid-template-columns:repeat(auto-fill,minmax(110px,1fr));gap:6px 10px;padding:10px 0}
.cmds label{display:flex;gap:8px;align-items:center;color:#d6e2de;font-size:.9rem}
button{padding:12px 16px;border:0;border-radius:8px;background:#3fbaa3;color:#06201b;font-size:1rem;font-weight:600}
button.g{background:#263230;color:#d6e2de}
#bar{position:sticky;bottom:0;background:#0f1514;padding:12px 0;display:flex;gap:10px;align-items:center;flex-wrap:wrap}
#msg{flex:1;min-width:140px}.ok{color:#3fbaa3}.bad{color:#e07a7a}
#rs{display:none;flex:1 1 100%;align-items:center;justify-content:space-between;gap:10px;background:#2a2412;border:1px solid #5a4a1a;color:#e0c07a;border-radius:10px;padding:8px 12px}
#rs button{padding:8px 12px;background:#e0c07a;color:#2a2412;white-space:nowrap}
#ov{display:none;position:fixed;inset:0;z-index:9;background:rgba(6,10,9,.88);align-items:center;justify-content:center;padding:16px}
.oc{background:#162120;border:1px solid #2d3d3a;border-radius:14px;padding:24px 22px;max-width:360px;width:100%;text-align:center}
.oc h2{margin:0 0 6px;font-size:1.1rem;letter-spacing:0;text-transform:none;color:#d6e2de}
#on{font-size:3.2rem;font-weight:700;color:#3fbaa3;line-height:1.2;min-height:1.2em}
#od{color:#9fb0aa;margin:6px 0 14px}
</style>
<div class=w>
<p><a href="/">&larr; Status</a> &middot; <a href="/nodes">Nodes</a></p>
<h1>Bot settings</h1>
<div id=form></div>
<div id=mq></div>
<h2>Firmware</h2>
<p><a href="/update">Firmware update (OTA)</a> &mdash; upload a new firmware file.</p>
<div id=bar><div id=rs><span>Restart needed to apply your changes.</span><button id=rb>Restart now</button></div><button id=save>Save</button><span id=msg></span></div>
</div>
<div id=ov><div class=oc><h2 id=ot></h2><div id=on></div><p id=od></p><button id=oc class=g>Cancel</button></div></div>
<script>
const $=i=>document.getElementById(i);
const S=[
 ["Identity",[
  ["name","Bot name","text","Shown to the mesh and used in replies."],
  ["lat","Latitude","num","Used by sun, moon and weather when the radio has no location."],
  ["lon","Longitude","num",""],
  ["tz","UTC offset (hours)","num","e.g. -6 for Mountain daylight time. Used for local times in replies."]]],
 ["Behavior",[
  ["answer_dm","Answer direct messages","sw",""],
  ["greeter","Greet new contacts","sw","Sends a welcome message the first time someone appears."],
  ["cmd_prefix","Command prefix","text","Optional, up to 3 characters (e.g. !). Leave empty for plain keywords."],
  ["allowlist","Channels to answer on","text","Comma-separated channel names, exactly as they appear in your app. Default is #bot. Leave empty to answer on every channel."]]],
 ["Weather",[["wx_metric","Units","sel","",[["1","Metric (°C, km/h)"],["0","Imperial (°F, mph)"]]]]],
 ["Rate limits (replies per minute)",[
  ["rl_global","Whole bot","int","Total replies the bot may send per minute."],
  ["rl_channel","Per channel","int",""],
  ["rl_sender","Per person","int",""]]]
];
let D={};
let M={},P=[];
function inp(id,type,ph){const e=document.createElement('input');e.type=type;e.id=id;if(ph)e.placeholder=ph;return e;}
function fld(label,el,hint){const d=document.createElement('div');d.className='f';const l=document.createElement('label');l.textContent=label;l.appendChild(el);d.appendChild(l);if(hint){const h=document.createElement('div');h.className='h';h.textContent=hint;d.appendChild(h);}return d;}
function buildMq(){
  const root=$('mq');root.textContent='';
  const h=document.createElement('h2');h.textContent='MQTT uplink';root.appendChild(h);
  const note=document.createElement('div');note.className='h';note.style.marginBottom='6px';
  note.textContent='Where this bot reports the packets it hears. A wrong broker only stops the uplink; the bot keeps answering commands. Changes apply after a restart.';
  root.appendChild(note);
  const c=document.createElement('div');c.className='card';
  const io=inp('m_iata','text');io.maxLength=4;io.style.textTransform='uppercase';
  c.appendChild(fld('Location code',io,'Your nearest airport code, 3 letters (e.g. YYC). Shown on the broker\'s map.'));
  c.appendChild(fld('Observer name',inp('m_origin','text'),'Optional. Leave empty to use the bot name.'));
  for(let i=1;i<=2;i++){
    const s=document.createElement('div');s.className='sl';
    const sel=document.createElement('select');sel.id='m'+i+'_preset';sel.onchange=()=>vis(i);
    s.appendChild(fld('Broker '+i,sel));
    s.appendChild(fld('Broker address',inp('m'+i+'_host','text','host or wss://host/path'),'Hostname, or a full mqtt://, mqtts://, ws:// or wss:// address.'));
    const pt=inp('m'+i+'_port','number');pt.min=0;pt.max=65535;
    s.appendChild(fld('Port',pt,'8883 = TLS, 443 = secure WebSocket, otherwise plain MQTT. Not needed if the address includes one.'));
    s.appendChild(fld('Token audience',inp('m'+i+'_aud','text'),'For brokers that authenticate with this device\'s signed token (JWT). Leave empty for username and password.'));
    s.appendChild(fld('Username',inp('m'+i+'_user','text')));
    s.appendChild(fld('Password',inp('m'+i+'_pass','password','')));
    const st=document.createElement('div');st.className='stt';st.id='m'+i+'_st';s.appendChild(st);
    c.appendChild(s);
  }
  root.appendChild(c);
  const rb=document.createElement('button');rb.className='g';rb.style.marginTop='12px';rb.textContent='Reset MQTT to defaults';
  rb.onclick=async()=>{if(!confirm('Reset all MQTT settings to the firmware defaults?'))return;await fetch('/api/mqtt/reset',{method:'POST'});await load();say('MQTT reset. Restart to apply.','ok');};
  root.appendChild(rb);
}
function vis(i){
  const v=$('m'+i+'_preset').value;
  const pr=P.find(x=>x.n==v);
  const custom=v=='custom',cred=custom||(pr&&pr.c);
  const show=(id,on)=>{$(id).closest('.f').style.display=on?'':'none';};
  show('m'+i+'_host',custom);show('m'+i+'_port',custom);show('m'+i+'_aud',custom);
  show('m'+i+'_user',cred);show('m'+i+'_pass',cred);
}
function fillMq(){
  $('m_iata').value=M.iata;$('m_origin').value=M.origin;
  M.slots.forEach((s,k)=>{
    const i=k+1,sel=$('m'+i+'_preset');
    if(!sel.options.length){
      [['none','Off'],['custom','Custom broker...']].forEach(o=>{const op=document.createElement('option');op.value=o[0];op.textContent=o[1];sel.appendChild(op);});
      P.forEach(p=>{const op=document.createElement('option');op.value=p.n;op.textContent=p.n;sel.appendChild(op);});
    }
    sel.value=s.preset;$('m'+i+'_host').value=s.host;$('m'+i+'_port').value=s.port||'';
    $('m'+i+'_aud').value=s.aud;$('m'+i+'_user').value=s.user;
    $('m'+i+'_pass').value='';$('m'+i+'_pass').placeholder=s.pass_set?'saved (type to replace)':'';
    vis(i);
  });
}
async function mqStatus(){
  try{
    const d=await(await fetch('/api/mqtt/status')).json();
    d.slots.forEach((s,k)=>{
      const e=$('m'+(k+1)+'_st');if(!e)return;
      const T={ok:'Connected',wait:'Waiting for settings',fail:'Failed to connect',disc:'Connecting...',off:'Not running',inactive:'Off'};
      e.textContent=(T[s.state]||s.state)+(s.state=='ok'?' · '+s.ok+' sent, '+s.err+' errors':'');
      e.className='stt '+(s.state=='ok'?'ok':(s.state=='fail'?'bad':''));
    });
  }catch(e){}
}

function build(){
  const f=$('form');f.textContent='';
  S.forEach(([title,fields])=>{
    const h=document.createElement('h2');h.textContent=title;f.appendChild(h);
    const c=document.createElement('div');c.className='card';
    fields.forEach(([k,l,t,hint,opts])=>{
      const d=document.createElement('div');d.className='f';
      let el;
      if(t=='sw'){d.className='f sw';const lb=document.createElement('label');lb.textContent=l;el=document.createElement('input');el.type='checkbox';d.append(lb,el);}
      else{
        const lb=document.createElement('label');lb.textContent=l;
        if(t=='sel'){el=document.createElement('select');opts.forEach(o=>{const op=document.createElement('option');op.value=o[0];op.textContent=o[1];el.appendChild(op);});}
        else{el=document.createElement('input');el.type=(t=='text')?'text':'number';if(t=='num')el.step='any';if(t=='int'){el.min=1;el.max=60;}}
        lb.appendChild(el);d.appendChild(lb);
      }
      el.id='f_'+k;
      if(hint){const p=document.createElement('div');p.className='h';p.textContent=hint;d.appendChild(p);}
      c.appendChild(d);
    });
    f.appendChild(c);
  });
  const h=document.createElement('h2');h.textContent='Commands';f.appendChild(h);
  const c=document.createElement('div');c.className='card';
  const g=document.createElement('div');g.className='cmds';g.id='cmds';c.appendChild(g);
  const p=document.createElement('div');p.className='h';p.style.paddingBottom='10px';p.textContent='Untick a command to switch it off.';c.appendChild(p);
  f.appendChild(c);
}
function fill(d){
  D=d;
  $('f_name').value=d.name;$('f_lat').value=d.lat;$('f_lon').value=d.lon;$('f_tz').value=d.tz_min/60;
  $('f_answer_dm').checked=!!d.answer_dm;$('f_greeter').checked=!!d.greeter;
  $('f_cmd_prefix').value=d.cmd_prefix;$('f_allowlist').value=d.allowlist;
  $('f_wx_metric').value=String(d.wx_metric);
  $('f_rl_global').value=d.rl_global;$('f_rl_channel').value=d.rl_channel;$('f_rl_sender').value=d.rl_sender;
  const off=d.disabled_cmds.split(',');const g=$('cmds');g.textContent='';
  d.commands.forEach(k=>{const l=document.createElement('label');const cb=document.createElement('input');cb.type='checkbox';cb.dataset.k=k;cb.checked=!off.includes(k);l.append(cb,k);g.appendChild(l);});
}
async function load(){fill(await(await fetch('/api/settings')).json());M=await(await fetch('/api/mqtt/config')).json();fillMq();setRs(needRestart());mqStatus();}
function say(t,c){const m=$('msg');m.textContent=t;m.className=c||'';}
function needRestart(){return !!(D.restart_needed||M.restart_needed);}
function setRs(on){$('rs').style.display=on?'flex':'none';}
let cd=null;
function overlay(title,num,desc,cancel){
  $('ot').textContent=title;$('on').textContent=num;$('od').textContent=desc;
  $('oc').style.display=cancel?'':'none';$('ov').style.display='flex';
}
function countdown(n){
  clearInterval(cd);
  const draw=()=>overlay('Settings saved',String(n),'Restarting the bot in '+n+' second'+(n==1?'':'s')+' to apply them.',true);
  draw();
  cd=setInterval(()=>{n--;if(n<=0){clearInterval(cd);restart();}else draw();},1000);
}
$('oc').onclick=()=>{clearInterval(cd);$('ov').style.display='none';setRs(true);say('Saved. Restart when you are ready.','ok');};
async function restart(){
  clearInterval(cd);
  overlay('Restarting the bot','\u2026','This takes about 20 seconds. You will be taken to the main page when it is back.',false);
  try{await fetch('/api/reboot',{method:'POST'});}catch(e){}
  let down=false;const t0=Date.now();
  const iv=setInterval(async()=>{
    let up=false;
    try{const c=new AbortController(),to=setTimeout(()=>c.abort(),2500);const r=await fetch('/api/settings',{signal:c.signal,cache:'no-store'});clearTimeout(to);up=r.ok;}catch(e){}
    if(!up)down=true;
    $('on').textContent=Math.round((Date.now()-t0)/1000)+'s';
    if((up&&down)||Date.now()-t0>90000){clearInterval(iv);location.href='/';}
  },2000);
}
$('save').onclick=async()=>{
  const dis=[...document.querySelectorAll('#cmds input')].filter(x=>!x.checked).map(x=>x.dataset.k).join(',');
  const b=new URLSearchParams({
    name:$('f_name').value,lat:$('f_lat').value,lon:$('f_lon').value,
    tz_min:String(Math.round(parseFloat($('f_tz').value)*60)),
    answer_dm:$('f_answer_dm').checked?1:0,greeter:$('f_greeter').checked?1:0,
    cmd_prefix:$('f_cmd_prefix').value,allowlist:$('f_allowlist').value,
    wx_metric:$('f_wx_metric').value,
    rl_global:$('f_rl_global').value,rl_channel:$('f_rl_channel').value,rl_sender:$('f_rl_sender').value,
    disabled_cmds:dis,
    mqtt_iata:$('m_iata').value.trim().toUpperCase(),mqtt_origin:$('m_origin').value});
  for(let i=1;i<=2;i++){
    const pre='mqtt'+i+'_';
    b.set(pre+'preset',$('m'+i+'_preset').value);b.set(pre+'host',$('m'+i+'_host').value.trim());
    b.set(pre+'port',$('m'+i+'_port').value);b.set(pre+'aud',$('m'+i+'_aud').value.trim());
    b.set(pre+'user',$('m'+i+'_user').value);b.set(pre+'pass',$('m'+i+'_pass').value);
  }
  say('Saving...');$('save').disabled=true;
  try{
    const r=await fetch('/api/settings',{method:'POST',body:b});
    if(!r.ok){say(await r.text(),'bad');await load();return;}
    await load();
    if(needRestart()){say('Saved.','ok');countdown(5);}
    else say('Saved. No restart needed.','ok');
  }catch(e){say('Could not reach the device.','bad');}
  finally{$('save').disabled=false;}
};
$('rb').onclick=restart;
(async()=>{build();buildMq();try{P=await(await fetch('/api/mqtt/presets')).json();}catch(e){}await load();setInterval(mqStatus,5000);})();
</script>
)HTML";
