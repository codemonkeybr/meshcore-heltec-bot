#pragma once

// Static settings page (flash only; the values come from /api/settings).
static const char SETTINGS_PAGE[] PROGMEM = R"HTML(<!doctype html><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Bot settings</title>
<link rel=stylesheet href="/ui.css">
<style>
h2{font-size:.76rem;letter-spacing:.08em;text-transform:uppercase;color:var(--mut);margin:0 0 .5rem}
#form{display:grid;gap:18px 16px;grid-template-columns:repeat(auto-fit,minmax(min(100%,440px),1fr));align-items:start}
section{margin-top:18px}#form section{margin-top:0}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:2px 14px}
.f{padding:11px 0;border-bottom:1px solid var(--line)}.f:last-child{border-bottom:0}
label{display:grid;grid-template-columns:1fr minmax(120px,44%);gap:4px 12px;align-items:center;font-weight:600}
input[type=text],input[type=number],input[type=password],select{width:100%;padding:9px 10px;background:var(--bg);color:inherit;border:1px solid var(--line);border-radius:9px;font:inherit;font-weight:400}
input:focus,select:focus{border-color:var(--acc);outline:0}
.h{color:var(--mut);font-size:.8rem;margin-top:3px;font-weight:400}
.f.sw{display:flex;flex-wrap:wrap;justify-content:space-between;align-items:center;gap:2px 12px}
.f.sw .h{flex:1 1 100%;order:3;margin-top:0}
input[type=checkbox]{appearance:none;-webkit-appearance:none;flex:none;width:42px;height:24px;border-radius:12px;background:var(--line);position:relative;cursor:pointer;margin:0;transition:background .15s}
input[type=checkbox]::after{content:"";position:absolute;top:3px;left:3px;width:18px;height:18px;border-radius:50%;background:#fff;box-shadow:0 1px 2px rgba(0,0,0,.35);transition:left .15s}
input[type=checkbox]:checked{background:var(--acc)}input[type=checkbox]:checked::after{left:21px}
.rg{display:flex;align-items:center;gap:10px}
input[type=range]{flex:1;min-width:0;accent-color:var(--acc);height:24px;margin:0}
.rg output{min-width:2ch;text-align:right;font-weight:650;font-variant-numeric:tabular-nums}
.sl{padding:12px 0;border-top:1px solid var(--line)}.sl:first-of-type{border-top:0}
#mqc{display:grid;gap:0 24px;grid-template-columns:repeat(auto-fit,minmax(min(100%,400px),1fr))}
.stt{font-size:.85rem;margin-top:6px;color:var(--mut)}.stt.ok{color:var(--acc)}.stt.bad{color:var(--bad)}
.cmds{display:flex;flex-wrap:wrap;gap:8px;padding:12px 0 4px}
.cmds label{display:block;cursor:pointer}
.cmds input{position:absolute;opacity:0;width:1px;height:1px}
.cmds span{display:block;padding:6px 14px;border-radius:999px;border:1px solid var(--line);color:var(--mut);font-weight:500;font-size:.9rem;text-decoration:line-through}
.cmds input:checked+span{background:color-mix(in srgb,var(--acc) 16%,transparent);border-color:color-mix(in srgb,var(--acc) 55%,transparent);color:var(--fg);text-decoration:none}
.cmds input:focus-visible+span{outline:2px solid var(--acc);outline-offset:2px}
.fw{display:flex;flex-wrap:wrap;gap:10px 16px;align-items:center;justify-content:space-between;padding:12px 0}
.lnk{display:inline-block;padding:9px 16px;border-radius:9px;background:var(--line);color:var(--fg);font-weight:600}
#bar{position:sticky;bottom:0;margin:22px -16px 0;padding:12px 16px calc(12px + env(safe-area-inset-bottom,0px));background:var(--card);border-top:1px solid var(--line);display:flex;gap:12px;align-items:center;flex-wrap:wrap;justify-content:flex-end}
#msg{flex:1;min-width:120px;order:-1}.ok{color:var(--acc)}.bad{color:var(--bad)}
#rs{display:none;flex:1 1 100%;order:-2;align-items:center;justify-content:space-between;gap:10px;background:color-mix(in srgb,var(--amb) 16%,var(--card));border:1px solid color-mix(in srgb,var(--amb) 45%,var(--line));color:var(--amb);border-radius:10px;padding:8px 12px}
#rs button{padding:7px 12px;background:var(--amb);color:var(--card);white-space:nowrap}
#ov{display:none;position:fixed;inset:0;z-index:9;background:rgba(6,10,9,.86);align-items:center;justify-content:center;padding:16px}
.oc{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:24px 22px;max-width:380px;width:100%;text-align:center}
.oc h2{margin:0 0 6px;font-size:1.1rem;letter-spacing:0;text-transform:none;color:var(--fg)}
#on{font-size:3.2rem;font-weight:700;color:var(--acc);line-height:1.2;min-height:1.2em}
#od{color:var(--mut);margin:6px 0 14px}
</style>
<div class=w>
<div class=top><div><h1>Settings <span class="pill w" id=pl style="display:none">Restart needed</span></h1><div class=sb>Changes apply after a restart.</div></div>
<nav class=nav><a href="/">Status</a><a href="/nodes">Nodes</a><a class=on href="/settings">Settings</a></nav></div>
<div id=form></div>
<section id=mq></section>
<section><h2>Firmware</h2><div class=card><div class=fw><div><div style="font-weight:600" id=fv>Firmware</div><div class=h>Upload a new firmware file (.bin) to update the bot.</div></div><a class=lnk href="/update">Update firmware</a></div></div></section>
<div id=bar><div id=rs><span>Restart needed to apply your changes.</span><button id=rb>Restart now</button></div><span id=msg></span><button id=save>Save</button></div>
</div>
<div id=ov><div class=oc><h2 id=ot></h2><div id=on></div><p id=od></p><button id=oc class=g2>Cancel</button></div></div>
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
  const note=document.createElement('div');note.className='h';note.style.marginBottom='8px';
  note.textContent='Where this bot reports the packets it hears. A wrong broker only stops the uplink; the bot keeps answering commands. Changes apply after a restart.';
  root.appendChild(note);
  const c=document.createElement('div');c.className='card';
  const io=inp('m_iata','text');io.maxLength=4;io.style.textTransform='uppercase';
  c.appendChild(fld('Location code',io,'Your nearest airport code, 3 letters (e.g. YYC). Shown on the broker\'s map.'));
  c.appendChild(fld('Observer name',inp('m_origin','text'),'Optional. Leave empty to use the bot name.'));
  const mc=document.createElement('div');mc.id='mqc';c.appendChild(mc);
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
    mc.appendChild(s);
  }
  root.appendChild(c);
  const rb=document.createElement('button');rb.className='g2';rb.style.marginTop='12px';rb.textContent='Reset MQTT to defaults';
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
    const sc=document.createElement('section');f.appendChild(sc);
    const h=document.createElement('h2');h.textContent=title;sc.appendChild(h);
    const c=document.createElement('div');c.className='card';
    fields.forEach(([k,l,t,hint,opts])=>{
      const d=document.createElement('div');d.className='f';
      let el;
      if(t=='sw'){d.className='f sw';const lb=document.createElement('label');lb.textContent=l;el=document.createElement('input');el.type='checkbox';d.append(lb,el);}
      else{
        const lb=document.createElement('label');lb.textContent=l;
        if(t=='sel'){el=document.createElement('select');opts.forEach(o=>{const op=document.createElement('option');op.value=o[0];op.textContent=o[1];el.appendChild(op);});}
        else if(t=='int'){el=document.createElement('input');el.type='range';el.min=1;el.max=60;const o=document.createElement('output');el.oninput=()=>{o.textContent=el.value;};const w=document.createElement('div');w.className='rg';w.append(el,o);lb.appendChild(w);d.appendChild(lb);}
        else{el=document.createElement('input');el.type=(t=='text')?'text':'number';if(t=='num')el.step='any';}
        if(t!='int'){lb.appendChild(el);d.appendChild(lb);}
      }
      el.id='f_'+k;
      if(hint){const p=document.createElement('div');p.className='h';p.textContent=hint;d.appendChild(p);}
      c.appendChild(d);
    });
    sc.appendChild(c);
  });
  const sc=document.createElement('section');f.appendChild(sc);
  const h=document.createElement('h2');h.textContent='Commands';sc.appendChild(h);
  const c=document.createElement('div');c.className='card';
  const g=document.createElement('div');g.className='cmds';g.id='cmds';c.appendChild(g);
  const p=document.createElement('div');p.className='h';p.style.paddingBottom='10px';p.textContent='Tap a command to switch it off. Struck-through commands are off.';c.appendChild(p);
  sc.appendChild(c);
}
function fill(d){
  D=d;
  $('f_name').value=d.name;$('f_lat').value=d.lat;$('f_lon').value=d.lon;$('f_tz').value=d.tz_min/60;
  $('f_answer_dm').checked=!!d.answer_dm;$('f_greeter').checked=!!d.greeter;
  $('f_cmd_prefix').value=d.cmd_prefix;$('f_allowlist').value=d.allowlist;
  $('f_wx_metric').value=String(d.wx_metric);
  $('f_rl_global').value=d.rl_global;$('f_rl_channel').value=d.rl_channel;$('f_rl_sender').value=d.rl_sender;
  const off=d.disabled_cmds.split(',');const g=$('cmds');g.textContent='';
  d.commands.forEach(k=>{const l=document.createElement('label');const cb=document.createElement('input');cb.type='checkbox';cb.dataset.k=k;cb.checked=!off.includes(k);const sp=document.createElement('span');sp.textContent=k;l.append(cb,sp);g.appendChild(l);});
  document.querySelectorAll('input[type=range]').forEach(e=>e.oninput());
}
async function ver(){try{$('fv').textContent='Firmware '+(await(await fetch('/api/status')).json()).version;}catch(e){}}
async function load(){fill(await(await fetch('/api/settings')).json());M=await(await fetch('/api/mqtt/config')).json();fillMq();setRs(needRestart());mqStatus();}
function say(t,c){const m=$('msg');m.textContent=t;m.className=c||'';}
function needRestart(){return !!(D.restart_needed||M.restart_needed);}
function setRs(on){$('rs').style.display=on?'flex':'none';$('pl').style.display=on?'inline-block':'none';}
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
(async()=>{build();buildMq();ver();try{P=await(await fetch('/api/mqtt/presets')).json();}catch(e){}await load();setInterval(mqStatus,5000);})();
</script>
)HTML";
