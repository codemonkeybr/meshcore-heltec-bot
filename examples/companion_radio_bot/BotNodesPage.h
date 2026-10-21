#pragma once

// Static nodes page (flash only). Data comes from /api/nodes (the node cache),
// /api/status (device clock + the bot's own location) - all sorting, grouping and
// filtering happens in the browser, so the bot does no extra work per view.
static const char NODES_PAGE[] PROGMEM = R"HTML(<!doctype html><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Nodes</title>
<style>
body{margin:0;background:#0f1514;color:#d6e2de;font:15px/1.4 system-ui,sans-serif}
.w{max-width:640px;margin:0 auto;padding:16px 16px 48px}
a{color:#3fbaa3}h1{font-size:1.25rem;margin:.4rem 0 .6rem}
.top{position:sticky;top:0;background:#0f1514;padding:8px 0 10px;z-index:2}
.tabs{display:flex;gap:6px;overflow-x:auto;padding-bottom:8px;-webkit-overflow-scrolling:touch}
.tab{flex:none;padding:7px 12px;border-radius:999px;background:#162120;border:1px solid #263230;color:#9fb0aa;font-size:.88rem}
.tab.on{background:#3fbaa3;border-color:#3fbaa3;color:#06201b;font-weight:600}
.row{display:flex;gap:8px}
input,select{box-sizing:border-box;padding:10px;background:#162120;color:inherit;border:1px solid #2d3d3a;border-radius:8px;font-size:1rem}
input{flex:1;min-width:0}
h2{font-size:.8rem;letter-spacing:.08em;text-transform:uppercase;color:#7d8a85;margin:1.4rem 0 .4rem}
.n{background:#162120;border:1px solid #263230;border-radius:10px;padding:10px 12px;margin:6px 0;display:grid;grid-template-columns:auto 1fr auto;gap:2px 10px;align-items:center}
.dot{width:10px;height:10px;border-radius:50%;background:#4a5552;grid-row:1/3}
.dot.g{background:#3fbaa3}.dot.a{background:#e0c07a}
.nm{font-weight:600;overflow-wrap:anywhere}
.ago{color:#d6e2de;font-variant-numeric:tabular-nums;text-align:right}
.sub{color:#7d8a85;font-size:.82rem;grid-column:2/4}
.sub span+span::before{content:" \00b7 "}
.empty{color:#7d8a85;padding:18px 0;text-align:center}
#foot{color:#7d8a85;font-size:.8rem;margin-top:16px}
</style>
<div class=w>
<p><a href="/">&larr; Status</a> &middot; <a href="/settings">Settings</a></p>
<h1>Nodes</h1>
<div class=top>
<div class=tabs id=tabs></div>
<div class=row><input id=q type=search placeholder="Search name or ID" autocomplete=off>
<select id=sort><option value=heard>Last heard</option><option value=name>Name</option><option value=dist>Distance</option></select></div>
</div>
<div id=list><div class=empty>Loading...</div></div>
<div id=foot></div>
</div>
<script>
const $=i=>document.getElementById(i);
const KINDS=[[1,'Companions'],[2,'Repeaters'],[3,'Rooms'],[4,'Sensors'],[0,'Other']];
let N=[],now=0,me=null,tab='all';
const kindOf=t=>(t>=1&&t<=4)?t:0;
function ago(s){
  if(s<45)return 'just now';
  if(s<3600)return Math.round(s/60)+' min ago';
  if(s<86400)return Math.round(s/3600)+' h ago';
  return Math.round(s/86400)+' d ago';
}
function km(a,b,c,d){
  const R=6371,r=x=>x*Math.PI/180,dl=r(c-a),dn=r(d-b);
  const h=Math.sin(dl/2)**2+Math.cos(r(a))*Math.cos(r(c))*Math.sin(dn/2)**2;
  return 2*R*Math.asin(Math.sqrt(h));
}
function prep(){
  N.forEach(n=>{
    n.age=n.last_heard?Math.max(0,now-n.last_heard):null;
    n.dist=(me&&n.has_loc)?km(me[0],me[1],n.lat,n.lon):null;
    n.k=kindOf(n.type);
  });
}
function card(n){
  const d=document.createElement('div');d.className='n';
  const dot=document.createElement('span');dot.className='dot'+(n.age===null?'':n.age<3600?' g':n.age<86400?' a':'');
  const nm=document.createElement('span');nm.className='nm';nm.textContent=n.name||'(no name)';
  const ag=document.createElement('span');ag.className='ago';ag.textContent=n.age===null?'not heard live':ago(n.age);
  const sub=document.createElement('span');sub.className='sub';
  const add=t=>{const s=document.createElement('span');s.textContent=t;sub.appendChild(s);return s;};
  add('ID '+n.hash.slice(0,2)).title='Repeaters appear in paths by this 1-byte ID. Full key prefix: '+n.hash;
  if(n.dist!==null)add(n.dist<10?n.dist.toFixed(1)+' km':Math.round(n.dist)+' km');
  if(n.has_loc){const a=document.createElement('a');a.textContent='map';a.target='_blank';a.rel='noopener';a.href='https://www.openstreetmap.org/?mlat='+n.lat+'&mlon='+n.lon+'#map=14/'+n.lat+'/'+n.lon;const s=document.createElement('span');s.appendChild(a);sub.appendChild(s);}
  else add('no location');
  d.append(dot,nm,ag,sub);return d;
}
function cmp(){
  const s=$('sort').value;
  if(s=='name')return(a,b)=>(a.name||'').localeCompare(b.name||'');
  if(s=='dist')return(a,b)=>(a.dist===null)-(b.dist===null)||a.dist-b.dist;
  return(a,b)=>(a.age===null)-(b.age===null)||a.age-b.age;
}
function render(){
  const q=$('q').value.trim().toLowerCase();
  const match=n=>!q||(n.name||'').toLowerCase().includes(q)||n.hash.startsWith(q);
  const tabs=$('tabs');tabs.textContent='';
  const mk=(id,label,count)=>{const b=document.createElement('button');b.className='tab'+(tab==id?' on':'');b.textContent=label+' '+count;b.onclick=()=>{tab=id;render();};tabs.appendChild(b);};
  mk('all','All',N.length);
  KINDS.forEach(([k,l])=>{const c=N.filter(n=>n.k==k).length;if(c)mk(k,l,c);});
  const list=$('list');list.textContent='';
  let shown=0;
  KINDS.forEach(([k,l])=>{
    if(tab!='all'&&tab!=k)return;
    const g=N.filter(n=>n.k==k&&match(n)).sort(cmp());
    if(!g.length)return;
    const h=document.createElement('h2');h.textContent=l+' ('+g.length+')';list.appendChild(h);
    g.forEach(n=>list.appendChild(card(n)));shown+=g.length;
  });
  if(!shown){const e=document.createElement('div');e.className='empty';e.textContent=N.length?'No matches.':'No nodes heard yet.';list.appendChild(e);}
}
async function load(){
  try{
    const [st,nd]=await Promise.all([fetch('/api/status').then(r=>r.json()),fetch('/api/nodes').then(r=>r.json())]);
    now=st.clock;
    const p=(st.loc||'').split(',').map(parseFloat);
    me=(p.length==2&&(p[0]||p[1]))?p:null;
    N=nd.nodes;prep();render();
    $('foot').textContent=nd.total+' nodes in the bot\'s memory (oldest are replaced when it fills). Green = heard in the last hour, amber = last 24 h. "Not heard live" means known from the contact list only.';
  }catch(e){$('list').innerHTML='<div class=empty>Could not load nodes.</div>';}
}
$('q').oninput=render;$('sort').onchange=render;
load();
setInterval(()=>{if(document.visibilityState=='visible')load();},60000);
</script>
)HTML";
