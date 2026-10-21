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
.cmds{display:grid;grid-template-columns:repeat(auto-fill,minmax(110px,1fr));gap:6px 10px;padding:10px 0}
.cmds label{display:flex;gap:8px;align-items:center;color:#d6e2de;font-size:.9rem}
button{padding:12px 16px;border:0;border-radius:8px;background:#3fbaa3;color:#06201b;font-size:1rem;font-weight:600}
button.g{background:#263230;color:#d6e2de}
#bar{position:sticky;bottom:0;background:#0f1514;padding:12px 0;display:flex;gap:10px;align-items:center;flex-wrap:wrap}
#msg{flex:1;min-width:140px}.ok{color:#3fbaa3}.bad{color:#e07a7a}
#rs{display:none;background:#2a2412;border:1px solid #5a4a1a;color:#e0c07a;border-radius:10px;padding:10px 14px;margin:12px 0}
#rs button{margin-top:8px;background:#e0c07a;color:#2a2412}
</style>
<div class=w>
<p><a href="/">&larr; Status</a></p>
<h1>Bot settings</h1>
<div id=rs>Some changes only take effect after a restart.<br><button id=rb>Restart now</button></div>
<div id=form></div>
<div id=bar><button id=save>Save</button><span id=msg></span></div>
</div>
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
  ["allowlist","Channels to answer on","text","Comma-separated channel names. Leave empty to answer on every channel."]]],
 ["Weather",[["wx_metric","Units","sel","",[["1","Metric (°C, km/h)"],["0","Imperial (°F, mph)"]]]]],
 ["Rate limits (replies per minute)",[
  ["rl_global","Whole bot","int","Total replies the bot may send per minute."],
  ["rl_channel","Per channel","int",""],
  ["rl_sender","Per person","int",""]]]
];
let D={};
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
  $('rs').style.display=d.restart_needed?'block':'none';
}
async function load(){fill(await(await fetch('/api/settings')).json());}
function say(t,c){const m=$('msg');m.textContent=t;m.className=c||'';}
$('save').onclick=async()=>{
  const dis=[...document.querySelectorAll('#cmds input')].filter(x=>!x.checked).map(x=>x.dataset.k).join(',');
  const b=new URLSearchParams({
    name:$('f_name').value,lat:$('f_lat').value,lon:$('f_lon').value,
    tz_min:String(Math.round(parseFloat($('f_tz').value)*60)),
    answer_dm:$('f_answer_dm').checked?1:0,greeter:$('f_greeter').checked?1:0,
    cmd_prefix:$('f_cmd_prefix').value,allowlist:$('f_allowlist').value,
    wx_metric:$('f_wx_metric').value,
    rl_global:$('f_rl_global').value,rl_channel:$('f_rl_channel').value,rl_sender:$('f_rl_sender').value,
    disabled_cmds:dis});
  say('Saving...');
  try{
    const r=await fetch('/api/settings',{method:'POST',body:b});
    if(!r.ok){say(await r.text(),'bad');await load();return;}
    say('Saved.','ok');await load();
  }catch(e){say('Could not reach the device.','bad');}
};
$('rb').onclick=async()=>{
  $('rb').disabled=true;
  try{await fetch('/api/reboot',{method:'POST'});}catch(e){}
  say('Restarting... this page reloads when the bot is back.');
  const t=setInterval(async()=>{try{const r=await fetch('/api/settings');if(r.ok){clearInterval(t);location.reload();}}catch(e){}},3000);
};
build();load();
</script>
)HTML";
