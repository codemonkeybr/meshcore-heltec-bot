#pragma once

// Static nodes page (flash only). Data comes from /api/nodes (the node cache),
// /api/status (device clock + the bot's own location) - all sorting, grouping and
// filtering happens in the browser, so the bot does no extra work per view.
static const char NODES_PAGE[] PROGMEM = R"HTML(<!doctype html><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Nodes</title>
<link rel=stylesheet href="/ui.css">
<style>
.hist{display:flex;align-items:flex-end;gap:10px;height:96px;margin-top:4px}
.hist div{flex:1;display:flex;flex-direction:column;justify-content:flex-end;align-items:center;gap:4px;height:100%;font-size:.72rem;color:var(--mut);min-width:0}
.hist b{display:block;width:100%;max-width:64px;border-radius:5px 5px 2px 2px;background:var(--acc);min-height:3px}
.hist em{font-style:normal;color:var(--fg);font-variant-numeric:tabular-nums;font-size:.8rem}
.bar{position:sticky;top:0;background:var(--bg);padding:10px 0 8px;z-index:2}
.tabs{display:flex;gap:6px;overflow-x:auto;padding-bottom:8px}
.tab{flex:none;padding:7px 14px;border-radius:999px;background:var(--card);border:1px solid var(--line);color:var(--mut);font-size:.88rem;font-weight:500}
.tab.on{background:var(--acc);border-color:var(--acc);color:var(--ink);font-weight:600}
.row{display:flex;gap:8px}
input,select{padding:10px;background:var(--card);color:inherit;border:1px solid var(--line);border-radius:9px;font:inherit}
input{flex:1;min-width:0}
h2{font-size:.76rem;letter-spacing:.08em;text-transform:uppercase;color:var(--mut);margin:1.4rem 0 .5rem}
.ng{display:grid;gap:8px;grid-template-columns:repeat(auto-fill,minmax(min(100%,330px),1fr))}
.n{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:10px 12px;display:grid;grid-template-columns:auto 1fr auto;gap:3px 10px;align-items:center;min-width:0}
.dot{width:10px;height:10px;border-radius:50%;background:var(--line);grid-row:1/3}
.dot.g{background:var(--acc)}.dot.a{background:var(--amb)}
.nm{font-weight:600;overflow-wrap:anywhere}
.ago{font-variant-numeric:tabular-nums;text-align:right;font-size:.88rem}
.sub{color:var(--mut);font-size:.8rem;grid-column:2/4;display:flex;gap:4px 12px;flex-wrap:wrap;align-items:center}
.dbar{display:inline-block;width:44px;height:5px;border-radius:3px;background:var(--line);vertical-align:middle;overflow:hidden;margin-left:6px}
.dbar i{display:block;height:100%;background:var(--acc)}
.empty{color:var(--mut);padding:18px 0;text-align:center}
#foot{color:var(--mut);font-size:.8rem;margin-top:18px}
</style>
<div class=w>
<div class=top><div><h1>Nodes <span class=pill id=cnt>...</span></h1><div class=sb id=sb>&nbsp;</div></div>
<nav class=nav><a href="/">Status</a><a class=on href="/nodes">Nodes</a><a href="/settings">Settings</a></nav></div>
<div class=cd><h4>When nodes were last heard</h4><div class=hist id=hist></div></div>
<div class=bar>
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
let N=[],now=0,me=null,tab='all',MAXD=50;
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
  if(n.dist!==null){const t=add(n.dist<10?n.dist.toFixed(1)+' km':Math.round(n.dist)+' km');const b=document.createElement('span');b.className='dbar';const f=document.createElement('i');f.style.width=Math.max(3,Math.min(100,n.dist/MAXD*100))+'%';b.appendChild(f);t.appendChild(b);}
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
function hist(){
  const B=[['under 1 h',0],['1 to 6 h',0],['6 to 24 h',0],['older',0],['not heard live',0]];
  N.forEach(n=>{B[n.age===null?4:n.age<3600?0:n.age<21600?1:n.age<86400?2:3][1]++;});
  const mx=Math.max(1,...B.map(b=>b[1])),h=$('hist');h.textContent='';
  B.forEach((b,i)=>{const d=document.createElement('div');const e=document.createElement('em');e.textContent=b[1];
    const bar=document.createElement('b');bar.style.height=Math.round(b[1]/mx*62)+'px';if(i>=3)bar.style.opacity=i==3?.45:.22;
    const l=document.createElement('span');l.textContent=b[0];d.append(e,bar,l);h.appendChild(d);});
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
    const gd=document.createElement('div');gd.className='ng';g.forEach(n=>gd.appendChild(card(n)));list.appendChild(gd);shown+=g.length;
  });
  if(!shown){const e=document.createElement('div');e.className='empty';e.textContent=N.length?'No matches.':'No nodes heard yet.';list.appendChild(e);}
}
async function load(){
  try{
    const [st,nd]=await Promise.all([fetch('/api/status').then(r=>r.json()),fetch('/api/nodes').then(r=>r.json())]);
    now=st.clock;
    const p=(st.loc||'').split(',').map(parseFloat);
    me=(p.length==2&&(p[0]||p[1]))?p:null;
    N=nd.nodes;prep();MAXD=Math.max(10,...N.map(n=>n.dist||0));hist();$('cnt').textContent=nd.total+' known';$('sb').textContent='Updated '+new Date().toLocaleTimeString();render();
    $('foot').textContent=nd.total+' nodes in the bot\'s memory (oldest are replaced when it fills). Green = heard in the last hour, amber = last 24 h. "Not heard live" means known from the contact list only.';
  }catch(e){$('list').innerHTML='<div class=empty>Could not load nodes.</div>';}
}
$('q').oninput=render;$('sort').onchange=render;
load();
setInterval(()=>{if(document.visibilityState=='visible')load();},60000);
</script>
)HTML";
