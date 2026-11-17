#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CoreXY Scanner</title>
<style>
body{font-family:system-ui,sans-serif;margin:0;background:#15181c;color:#e6e6e6}
main{max-width:1400px;margin:auto;padding:12px;display:grid;gap:12px;grid-template-columns:repeat(6,1fr);align-items:start}
#s-status,#d-scan,#d-exp{grid-column:span 2}#d-man,#d-ota{grid-column:span 3}#s-prev{grid-column:1/-1}
@media(max-width:900px){main{grid-template-columns:1fr}main>*{grid-column:1/-1!important}}
section{background:#1f242a;border-radius:8px;padding:12px}
h2{font-size:15px;margin:0 0 8px;color:#8fc1ff}
label{display:flex;justify-content:space-between;align-items:center;margin:4px 0;gap:8px;font-size:14px}
input,select{width:120px;background:#111;color:#eee;border:1px solid #444;border-radius:4px;padding:4px}
input[type=checkbox]{width:auto}
button{background:#2d6cdf;color:#fff;border:0;border-radius:4px;padding:7px 11px;margin:2px;cursor:pointer}
button.red{background:#c0392b}button.gr{background:#444}
.kv{display:grid;grid-template-columns:auto 1fr;gap:2px 12px;font-family:monospace;font-size:13px;margin:6px 0}
.jog{display:grid;grid-template-columns:repeat(3,64px);gap:4px;justify-content:center;margin:8px 0}
progress{width:100%}canvas{width:100%;image-rendering:pixelated;background:#000;border-radius:4px}
details{background:#1f242a;border-radius:8px;padding:8px 12px}
summary{cursor:pointer;color:#8fc1ff;font-weight:600;font-size:15px;padding:4px 0}
details[open] summary{margin-bottom:6px}
.bar{position:sticky;top:0;z-index:5;background:#0f1215;border-bottom:1px solid #333;display:flex;gap:14px;align-items:center;padding:6px 12px;font-family:monospace}
.bar b{font-family:system-ui;color:#8fc1ff}.bar button{margin-left:auto}
.pbw{position:relative;margin:4px 0}.pbw progress{display:block;width:100%;height:20px}
.pbw span{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;font-size:12px;font-weight:600;color:#fff;text-shadow:0 0 3px #000}
#bpause{min-width:80px}
#big{font-size:72px;font-weight:800;color:#ff2a2a;text-align:center;line-height:1.1;margin:6px 0;font-variant-numeric:tabular-nums}
#msg{color:#ffb347;min-height:1.2em;font-size:13px}.wide{grid-column:1/-1}
</style></head><body>
<div class="bar"><b>CoreXY Scanner</b><span id="bstate">-</span><span id="bpos">-</span><button id="bpause" class="gr" onclick="cmd('/api/pause')" style="margin-left:auto">PAUSE</button><button class="red" style="margin-left:0" onclick="cmd('/api/stop')">STOP</button></div>
<main>
<section id="s-status"><h2>Status</h2><div class="kv">
<span>Zustand</span><span id="state">-</span>
<span>X / Y</span><span id="pos">-</span>
<span>Abstand</span><span id="dist">-</span>
<span>Sensor roh</span><span id="raw">-</span>
<span>Motoren</span><span id="en">-</span>
<span>Punkte</span><span id="prog">-</span>
<span>Wandler / Kal.</span><span id="adc">-</span>
<span>Scan-Speicher</span><span id="fs">-</span>
<span>Uhrzeit ESP</span><span id="clk">-</span>
<span>Firmware</span><span id="fw">-</span></div>
<div class="pbw"><progress id="pb" value="0" max="1"></progress><span id="pbt">0 %</span></div>
<div id="big">– %</div>
<div id="tinfo" class="kv"></div><div id="msg"></div>
<button class="red" onclick="cmd('/api/stop')">STOP</button>
<button class="gr" onclick="cmd('/api/enable?on=1')">Motoren an</button>
<button class="gr" onclick="cmd('/api/enable?on=0')">Motoren aus</button>
</section>
<details id="d-scan" open><summary>Scan-Einstellungen (ab aktueller Position)</summary>
<label>Bereich X mm<input id="w" type="number" value="50" step="any"></label>
<label>Bereich Y mm<input id="h" type="number" value="50" step="any"></label>
<label>Auflösung XY mm<input id="res" type="number" value="1" step="any"></label>
<label>Scan-Geschw. mm/s<input id="feed" type="number" value="20"></label>
<label>Eilgang mm/s<input id="travel" type="number" value="40"></label>
<label>Beruhigung ms<input id="settle" type="number" value="60"></label>
<label>Mittelung (Samples)<input id="avg" type="number" value="32"></label>
<label>Serpentine (bidirektional)<input id="bidir" type="checkbox" checked></label>
<label>Fliegende Messung (ohne Anhalten)<input id="fly" type="checkbox"></label>
<label>Sensor-Verzögerung ms (nur fliegend)<input id="lat" type="number" value="2" step="any"></label>
<div id="est" class="kv"></div>
<button class="gr" onclick="frame()">Rahmen abfahren</button>
<button onclick="startScan()">Scan starten</button>
<button id="spause" class="gr" onclick="cmd('/api/pause')">PAUSE</button>
<button class="red" onclick="cmd('/api/stop')">Abbrechen</button>
</details>
<details id="d-exp" open><summary>Export-Einstellungen</summary>
<label>Z-Null (Auflagefläche)<select id="bm" onchange="draw()"><option value="auto">Auto: Ebene fitten</option><option value="start">Messwert am Startpunkt</option><option value="manual">Manueller Abstand</option></select></label>
<label>Manueller Abstand mm<input id="dman" type="number" value="100" step="any" onchange="draw()"></label>
<label>Ebenen-Toleranz mm<input id="tol" type="number" value="0.3" step="any" onchange="draw()"></label>
<label>Mindesthöhe Teil mm (darunter = Auflage)<input id="thr" type="number" value="0.5" step="any" onchange="draw()"></label>
<h2 style="margin-top:12px">Fehlstellen</h2>
<label>Kleine Löcher schließen<input id="hf" type="checkbox" checked onchange="draw()"></label>
<label>max. Lochfläche mm² (größer = echtes Loch)<input id="hfa" type="number" value="4" step="any" onchange="draw()"></label>
<label>Ausreißer entfernen ab mm (0 = aus)<input id="ol" type="number" value="0" step="any" onchange="draw()"></label>
<h2 style="margin-top:12px">Oberfläche glätten</h2>
<label>Modus<select id="dzm" onchange="draw()"><option value="gauss">Gauß, kantenerhaltend (alle Exporte)</option><option value="avg">Mittelwert (alle Exporte)</option><option value="del">Löschen (nur CSV)</option></select></label>
<label>Z-Toleranz mm (0 = aus, Kanten > 3× bleiben)<input id="dz" type="number" value="0" step="any" onchange="draw()"></label>
<label>Gauß-Radius σ mm<input id="gs" type="number" value="0.5" step="any" onchange="draw()"></label>
<label>Gauß-Durchläufe<input id="gp" type="number" value="1" min="1" onchange="draw()"></label>
<label>Mittelwert-Radius (Punkte)<input id="dzr" type="number" value="1" min="1" onchange="draw()"></label>
<label>Ebene Flächen begradigen<input id="fl" type="checkbox" onchange="draw()"></label>
<label>Ebenen-Toleranz mm<input id="flt" type="number" value="0.05" step="any" onchange="draw()"></label>
<label>Ebene min. Fläche mm²<input id="fla" type="number" value="25" step="any" onchange="draw()"></label>
<h2 style="margin-top:12px">Umriss</h2>
<label>Umriss-Glättung σ mm (0 = aus)<input id="cs" type="number" value="0.5" step="any" onchange="draw()"></label>
<label>Konturen kleiner als mm² ignorieren<input id="cma" type="number" value="2" step="any" onchange="draw()"></label>
<label>Umriss in Vorschau zeigen<input id="cpv" type="checkbox" onchange="draw()"></label>
<label>Ausdünnung 3MF/STL/STEP (jeder k-te Punkt)<input id="dec" type="number" value="1" min="1"></label>
<label>Dateiname<input id="fn" value="scan"></label>
<button onclick="exp('csv')">CSV</button><button onclick="exp('3mf')">3MF</button><button onclick="exp('stl')">STL</button><button onclick="exp('step')">STEP</button><button onclick="exp('dxf')">Umriss DXF</button><button onclick="exp('svg')">Umriss SVG</button>
<div id="einfo" class="kv"></div>
</details>
<details id="d-man"><summary>Manuell verfahren</summary>
<label>Schrittweite mm<select id="js"><option>0.1</option><option>1</option><option selected>10</option><option>50</option></select></label>
<label>Geschwindigkeit mm/s<input id="jf" type="number" value="30"></label>
<div class="jog"><span></span><button onclick="jog(0,1)">Y+</button><span></span>
<button onclick="jog(-1,0)">X−</button><span></span><button onclick="jog(1,0)">X+</button>
<span></span><button onclick="jog(0,-1)">Y−</button><span></span></div>
<button onclick="cmd('/api/zero')">Nullpunkt hier</button>
<button onclick="cmd('/api/home?f='+$('jf').value)">Zum Nullpunkt</button>
<h2 style="margin-top:14px">Höhen-Kalibrierung (2 Punkte)</h2>
<label>Höhe Endmaß mm<input id="calH" type="number" value="10" step="any"></label>
<label>Messwerte pro Punkt<input id="calN" type="number" value="128"></label>
<button onclick="calP(1)">1: Auflage messen</button><button onclick="calP(2)">2: Endmaß messen</button><br>
<button onclick="calApply()">Übernehmen</button><button class="gr" onclick="if(confirm('Kalibrierung zurücksetzen?'))cmd('/api/cal?reset=1')">Zurücksetzen</button>
<div id="calI" class="kv"></div>
</details>
<details id="d-ota"><summary>Firmware-Update (OTA)</summary>
<input id="otaf" type="file" accept=".bin" style="width:auto">
<button onclick="ota()">Hochladen &amp; flashen</button>
<progress id="otap" value="0" max="1"></progress><div id="otat" class="kv"></div>
<button class="gr" onclick="if(confirm('Scan-Speicher formatieren? Der gespeicherte Scan wird gelöscht.'))cmd('/api/format').then(()=>{grid=null;draw();})">Scan-Speicher formatieren</button>
</details>

<section id="s-prev"><h2>Vorschau</h2>
<label style="justify-content:flex-start">Nur Teil anzeigen (Z-Null/Schwelle aus Export)<input id="pv" type="checkbox" onchange="draw()"></label>
<canvas id="cv" width="10" height="10"></canvas><div id="leg" class="kv"></div>
<button class="gr" onclick="loadData()">Daten neu laden</button>
</section>
</main>
<script>
const $=id=>document.getElementById(id);
let st={},grid=null,lastFetch=0,prevState='';
const fx=(v,n=3)=>v==null||!isFinite(v)?'–':(+v).toFixed(n);
function msg(t){$('msg').textContent=t||'';}
async function cmd(u){try{const r=await fetch(u,{method:'POST'});const t=await r.text();if(!r.ok)throw new Error(t);msg('');}catch(e){msg(e.message);}}
function jog(sx,sy){const s=+$('js').value;cmd(`/api/jog?dx=${sx*s}&dy=${sy*s}&f=${$('jf').value}`);}
function startScan(){const q=['w','h','res','feed','travel','settle','avg'].map(k=>k+'='+encodeURIComponent($(k).value)).join('&')+'&bidir='+($('bidir').checked?1:0)+'&fly='+($('fly').checked?1:0)+'&lat='+encodeURIComponent($('lat').value);grid=null;cmd('/api/scan?'+q);}
function frame(){cmd(`/api/frame?w=${encodeURIComponent($('w').value)}&h=${encodeURIComponent($('h').value)}&f=${encodeURIComponent($('travel').value)}`);}
// Zeiten: ESP liefert Laufzeit (el) und Zeit seit Ende (ago), Uhrzeit rechnet der Browser
const clk=ms=>new Date(ms).toLocaleString('de-DE',{weekday:'short',hour:'2-digit',minute:'2-digit'});
function timeInfo(){const pct=st.total?100*st.done/st.total:0,run=st.state==='SCAN'||st.state==='RETURN'||st.state==='PAUSED';
 $('big').textContent=st.total?pct.toFixed(1)+' %':'– %';
 const hasE=st.e0>0;if(!st.el&&!hasE){$('tinfo').innerHTML='';return;}
 const el=st.el?st.el/1000:(st.e1?st.e1-st.e0:0),now=Date.now();
 const t0=hasE?st.e0*1000:now-(st.ago||0)-st.el;   // Startzeit vom ESP (NTP), sonst aus Laufzeit
 const rem=run?(st.done>0?el*(st.total-st.done)/st.done:NaN):0,tot=el+rem;
 const end=run?now+rem*1000:(st.e1?st.e1*1000:t0+el*1000);
 const f=v=>isFinite(v)?fmtT(v):'–';
 $('tinfo').innerHTML=`<span>Start</span><span>${clk(t0)}</span><span>${run?'Fertig ca.':'Fertig um'}</span><span>${isFinite(end)?clk(end):'–'}</span>`+
  `<span>Gesamtdauer${run?' ca.':''}</span><span>${f(tot)}</span><span>Geschafft</span><span>${f(el)}</span><span>Restzeit</span><span>${f(rem)}</span>`;}
let cal={};
async function calP(n){try{$('calI').innerHTML='<span>Messe …</span><span></span>';
 const r=await fetch('/api/measure?n='+(+$('calN').value||128),{method:'POST'});if(!r.ok)throw new Error(await r.text());
 const j=await r.json();if(j.dnom==null)throw new Error('Messwert ungültig');cal[n]=j.dnom;calShow();}catch(e){msg(e.message);calShow();}}
function calShow(){const H=+$('calH').value;let h=`<span>Punkt 1 (Auflage)</span><span>${fx(cal[1],4)} mm</span><span>Punkt 2 (Endmaß)</span><span>${fx(cal[2],4)} mm</span>`;
 if(cal[1]!=null&&cal[2]!=null){const dn=cal[1]-cal[2];h+=`<span>Höhe gemessen / soll</span><span>${fx(dn,4)} / ${fx(H,4)} mm</span><span>neuer Faktor</span><span>${fx(H/dn,5)}</span>`;}
 $('calI').innerHTML=h;}
function calApply(){const H=+$('calH').value;if(cal[1]==null||cal[2]==null)return msg('Erst beide Punkte messen');
 const dn=cal[1]-cal[2];if(!(dn>0.5))return msg('Endmaß muss näher am Sensor sein als die Auflage (mind. 0,5 mm)');
 const sc=H/dn,of=cal[1]*(1-sc);cmd(`/api/cal?s=${sc.toFixed(6)}&o=${of.toFixed(4)}`).then(()=>{cal={};calShow();});}
function fmtT(s){s=Math.round(s);const h=Math.floor(s/3600),m=Math.floor(s%3600/60);return(h?h+' h ':'')+m+' min '+(s%60)+' s';}
function est(){const w=+$('w').value,h=+$('h').value,r=+$('res').value;if(!(r>0)){$('est').innerHTML='';return;}
 const nx=Math.floor(w/r+1e-4)+1,ny=Math.floor(h/r+1e-4)+1,n=nx*ny,f=+$('feed').value||1,a=st.acc||300;
 const ads=st.adc==='ADS1115',fl=$('fly').checked,ta=ads?(fl?0:(+$('avg').value)*1.25e-3):(+$('avg').value)*6e-5,tr=+$('travel').value||40;let t,warn='';
 if($('fly').checked){const ru=f*f/(2*a)+0.5,bi=$('bidir').checked;
  t=ny*((w+2*ru)/f+f/a+(bi?0:(w+2*ru)/tr+tr/a)+0.05);
  if(ads?r/f<0.0025:r/f<ta+0.002)warn=`<span>Warnung</span><span>Punktabstand ${(1000*r/f).toFixed(1)} ms &lt; Messzeit – Geschw. oder Mittelung senken</span>`;}
 else{const tm=r<f*f/a?2*Math.sqrt(r/a):r/f+f/a;t=n*(tm+(+$('settle').value)/1000+ta+0.003);}
 $('est').innerHTML=`<span>Raster</span><span>${nx} × ${ny} = ${n} Punkte</span><span>Dauer ca.</span><span>${fmtT(t)}</span>`+warn;}
async function poll(){
 try{const r=await fetch('/api/status');st=await r.json();
  const sn=st.state+(st.partial&&st.state==='DONE'?' (abgebrochen)':'');$('state').textContent=sn;$('bstate').textContent=sn;
  const zl=zP&&st.dist!=null?zP[0]*st.x+zP[1]*st.y+zP[2]-st.dist:NaN;
  $('bpos').textContent='X '+fx(st.x,2)+'  Y '+fx(st.y,2)+'  Z '+fx(zl,2);
  $('pos').textContent=fx(st.x)+' / '+fx(st.y)+' mm';
  $('dist').textContent=st.dist==null?'ungültig / kein Objekt':fx(st.dist)+' mm';
  $('raw').textContent=fx(st.raw)+(st.mode==='I'?' mA':' V');
  $('en').textContent=st.en?'an':'aus';
  $('prog').textContent=st.done+' / '+st.total;$('fw').textContent=st.fw||'-';$('clk').textContent=st.now?new Date(st.now*1000).toLocaleString('de-DE'):'noch keine NTP-Zeit';
  $('adc').textContent=(st.adc||'-')+'  ·  Faktor '+fx(st.cs,5)+', Offset '+fx(st.co,3)+' mm';
  if(st.fstotal)$('fs').textContent=`${st.fstotal>>10} kB gesamt, max. ca. ${Math.max(0,Math.floor((st.fstotal*0.95-16384)/2)).toLocaleString('de-DE')} Punkte`;
  $('pb').max=st.total||1;$('pb').value=st.done;$('pbt').textContent=(st.total?100*st.done/st.total:0).toFixed(1)+' %';timeInfo();
  const run=s=>s==='SCAN'||s==='RETURN'||s==='PAUSED';
  const pb=st.state==='PAUSED'?'WEITER':'PAUSE',pe=st.state==='SCAN'||st.state==='PAUSED';
  for(const id of['bpause','spause']){$(id).textContent=pb;$(id).disabled=!pe;$(id).style.opacity=pe?1:.4;}
  if(st.state==='SCAN'&&Date.now()-lastFetch>3000)loadData();
  if((run(prevState)&&!run(st.state))||(!grid&&st.total>0&&!run(st.state)&&Date.now()-lastFetch>5000))loadData();
  prevState=st.state;
 }catch(e){$('state').textContent='keine Verbindung';}
 est();setTimeout(poll,500);}
// lädt nur neue Zeilen nach (Server liefert Zeilen ab 'from')
async function loadData(){lastFetch=Date.now();try{const nx=st.nx,ny=st.ny,res=st.res;if(!nx||!ny)return;
 if(!grid||grid.nx!==nx||grid.ny!==ny||grid.res!==res||(st.rows??0)<grid.rows)grid={nx,ny,res,d:new Float32Array(nx*ny).fill(NaN),rows:0};
 const r=await fetch('/api/data?from='+grid.rows);if(!r.ok)return;
 const from=+r.headers.get('X-Row-From'),to=+r.headers.get('X-Row-To'),q=new Int16Array(await r.arrayBuffer());
 if(q.length!==(to-from)*nx||to>ny)return;
 for(let k=0;k<q.length;k++)grid.d[from*nx+k]=q[k]===-32768?NaN:q[k]/100;
 grid.rows=to;grid.ver=(grid.ver||0)+1;draw();}catch(e){}}
function cmap(t){const c=x=>Math.max(0,Math.min(1,x))*255;return[c(1.5-Math.abs(4*t-3)),c(1.5-Math.abs(4*t-2)),c(1.5-Math.abs(4*t-1))];}
function draw(){updZ();if(!grid)return;const{nx,ny,d}=grid;let v=d,lbl='Abstand';
 if($('pv').checked){try{v=heights().z;lbl='Höhe';}catch(e){msg(e.message);}}
 let mn=Infinity,mx=-Infinity;for(const x of v)if(isFinite(x)){if(x<mn)mn=x;if(x>mx)mx=x;}
 const off=document.createElement('canvas');off.width=nx;off.height=ny;const ox=off.getContext('2d'),img=ox.createImageData(nx,ny);
 for(let j=0;j<ny;j++)for(let i=0;i<nx;i++){const val=v[j*nx+i],p=((ny-1-j)*nx+i)*4;let c=[45,45,45];
  if(isFinite(val)){let t=(val-mn)/((mx-mn)||1);if(v===d)t=1-t;c=cmap(t);}img.data[p]=c[0];img.data[p+1]=c[1];img.data[p+2]=c[2];img.data[p+3]=255;}
 ox.putImageData(img,0,0);const cv=$('cv'),sc=Math.max(1,Math.floor(900/Math.max(nx,ny)));cv.width=nx*sc;cv.height=ny*sc;
 const g=cv.getContext('2d');g.imageSmoothingEnabled=false;g.drawImage(off,0,0,cv.width,cv.height);
 if($('cpv').checked){try{const C=contours(),r=grid.res;g.lineWidth=Math.max(1.5,sc*0.3);
  for(const L of C.loops){g.strokeStyle=L.hole?'#00e5ff':'#ffffff';g.beginPath();
   L.pts.forEach(([x,y],k)=>{const px=(x/r+0.5)*sc,py=(ny-1-y/r+0.5)*sc;k?g.lineTo(px,py):g.moveTo(px,py);});g.closePath();g.stroke();}}catch(e){msg(e.message);}}
 $('leg').innerHTML=`<span>${lbl}</span><span>${fx(mn)} … ${fx(mx)} mm  (blau = tief, rot = hoch, grau = leer)</span>`;}

// ---------- Z-Null / Höhen ----------
function plane(X,Y,D,sel){let n=0,sx=0,sy=0,sd=0;for(let k=0;k<D.length;k++)if(sel[k]){n++;sx+=X[k];sy+=Y[k];sd+=D[k];}
 if(n<3)return null;const mx=sx/n,my=sy/n,md=sd/n;let xx=0,xy=0,yy=0,xd=0,yd=0;
 for(let k=0;k<D.length;k++)if(sel[k]){const x=X[k]-mx,y=Y[k]-my,dd=D[k]-md;xx+=x*x;xy+=x*y;yy+=y*y;xd+=x*dd;yd+=y*dd;}
 const det=xx*yy-xy*xy;let a=0,b=0;
 if(Math.abs(det)>1e-9*(xx*yy+1e-12)){a=(xd*yy-yd*xy)/det;b=(yd*xx-xd*xy)/det;}else if(xx>1e-12)a=xd/xx;else if(yy>1e-12)b=yd/yy;
 return[a,b,md-a*mx-b*my];}
let fbCache=null;
function fitBase(){const{nx,ny,res,d}=grid,tol=Math.max(0.01,+$('tol').value);const fk=grid.ver+'|'+nx+'|'+ny+'|'+tol;if(fbCache&&fbCache.k===fk)return fbCache.P;
 const P=fitBase0(nx,ny,res,d,tol);fbCache={k:fk,P};return P;}
function fitBase0(nx,ny,res,d,tol){const X=[],Y=[],D=[];
 for(let j=0;j<ny;j++)for(let i=0;i<nx;i++){const v=d[j*nx+i];if(isFinite(v)){X.push(i*res);Y.push(j*res);D.push(v);}}
 if(D.length<3)throw new Error('Zu wenig gültige Punkte');
 const s=Float64Array.from(D).sort(),p95=s[Math.floor(0.95*(s.length-1))];
 let P=[0,0,p95],sel=D.map(v=>v>=p95-3*tol);
 for(let it=0;it<10;it++){const Q=plane(X,Y,D,sel);if(!Q)break;P=Q;sel=D.map((v,k)=>Math.abs(P[0]*X[k]+P[1]*Y[k]+P[2]-v)<tol);}
 return P;}
let zP=null;  // Z-Null-Ebene für Live-Z in der Kopfleiste
function baseP(){const m=$('bm').value;if(m==='manual')return[0,0,+$('dman').value];
 if(!grid)throw new Error('Keine Scandaten');if(m==='start'){if(!isFinite(grid.d[0]))throw new Error('Messwert am Startpunkt ungültig');return[0,0,grid.d[0]];}
 return fitBase();}
function updZ(){try{zP=baseP();}catch(e){zP=null;}}
const HP=['bm','dman','tol','thr','hf','hfa','ol','dz','dzm','dzr','gs','gp','fl','flt','fla'];
let hCache=null;
function heights(){if(!grid)throw new Error('Keine Scandaten');
 const key=grid.ver+'|'+grid.nx+'|'+grid.ny+'|'+HP.map(k=>$(k).type==='checkbox'?$(k).checked:$(k).value).join('|');
 if(hCache&&hCache.key===key)return hCache.H;
 const{nx,ny,res,d}=grid,thr=Math.max(0.01,+$('thr').value);const P=baseP();
 let z=new Float32Array(nx*ny);
 for(let j=0;j<ny;j++)for(let i=0;i<nx;i++){const k=j*nx+i,dv=d[k];z[k]=isFinite(dv)?P[0]*i*res+P[1]*j*res+P[2]-dv:NaN;}
 const ol=+$('ol').value;let nOut=0,nHole=0;
 if(ol>0){const r=removeOutliers(z,nx,ny,ol);z=r.z;nOut=r.n;}
 if($('hf').checked){const r=fillHoles(z,nx,ny,res,thr,Math.max(0,+$('hfa').value));z=r.z;nHole=r.n;}
 const tz=+$('dz').value,m=$('dzm').value;
 if(tz>0&&m==='avg')z=smooth(z,nx,ny,tz,Math.max(1,Math.round(+$('dzr').value)||1));
 if(tz>0&&m==='gauss'){const sg=Math.max(0.01,+$('gs').value),np=Math.max(1,Math.round(+$('gp').value)||1);for(let q=0;q<np;q++)z=gauss(z,nx,ny,res,sg,tz);}
 if($('fl').checked)z=flatten(z,nx,ny,res,Math.max(0.001,+$('flt').value),Math.max(0,+$('fla').value));
 const zr=Float32Array.from(z);          // geglättet, ohne Schwelle (für Umriss)
 let cnt=0;for(let k=0;k<z.length;k++){if(z[k]>=thr)cnt++;else z[k]=NaN;}
 const H={z,zr,P,cnt,thr,nOut,nHole};hCache={key,H};return H;}
// Ausreißer: Einzelpunkt, der von ALLEN Nachbarn um mehr als lim abweicht -> ungültig (wird danach gefüllt).
// Kantenpunkte haben immer Nachbarn auf gleicher Höhe und bleiben erhalten.
function removeOutliers(z,nx,ny,lim){const o=Float32Array.from(z);let n=0;
 for(let j=0;j<ny;j++)for(let i=0;i<nx;i++){const c=z[j*nx+i];if(c!==c)continue;let dmin=Infinity,cnt=0;
  for(let v=j-1;v<=j+1;v++)for(let u=i-1;u<=i+1;u++){if((u===i&&v===j)||u<0||v<0||u>=nx||v>=ny)continue;const h=z[v*nx+u];if(h===h){cnt++;dmin=Math.min(dmin,Math.abs(h-c));}}
  if(cnt>=3&&dmin>lim){o[j*nx+i]=NaN;n++;}}
 return{z:o,n};}
// Löcher im Teil schließen: zusammenhängende Bereiche ohne Teil-Messwert (ungültig oder unter Mindesthöhe),
// die den Scanrand nicht berühren und kleiner als maxA sind, werden glatt interpoliert (Laplace).
function fillHoles(z,nx,ny,res,thr,maxA){const n=nx*ny,o=Float32Array.from(z),lab=new Uint8Array(n),q=new Int32Array(n),maxN=maxA/(res*res);let cnt=0;
 const part=k=>o[k]>=thr;
 for(let s0=0;s0<n;s0++){if(lab[s0]||part(s0))continue;
  let head=0,tail=0,border=false;q[tail++]=s0;lab[s0]=1;
  while(head<tail){const c=q[head++],ci=c%nx,cj=(c-ci)/nx;if(ci===0||cj===0||ci===nx-1||cj===ny-1)border=true;
   if(ci>0){const k=c-1;if(!lab[k]&&!part(k)){lab[k]=1;q[tail++]=k;}}
   if(ci<nx-1){const k=c+1;if(!lab[k]&&!part(k)){lab[k]=1;q[tail++]=k;}}
   if(cj>0){const k=c-nx;if(!lab[k]&&!part(k)){lab[k]=1;q[tail++]=k;}}
   if(cj<ny-1){const k=c+nx;if(!lab[k]&&!part(k)){lab[k]=1;q[tail++]=k;}}}
  if(border||tail>maxN)continue;
  const H=q.slice(0,tail),hs=new Set(H);
  // Startwert: Mittel der umgebenden Teilpunkte
  let s=0,m=0;for(const c of H)for(const k of[c-1,c+1,c-nx,c+nx])if(!hs.has(k)&&part(k)){s+=o[k];m++;}
  const z0=m?s/m:thr;for(const c of H)o[c]=z0;
  for(let it=0;it<Math.min(2000,20+4*tail);it++){let dmax=0;
   for(const c of H){const v=(o[c-1]+o[c+1]+o[c-nx]+o[c+nx])/4;dmax=Math.max(dmax,Math.abs(v-o[c]));o[c]=v;}
   if(dmax<1e-4)break;}
  cnt++;}
 return{z:o,n:cnt};}
// Kantenerhaltendes Gauß-Glätten (bilateral): Abstandsgewicht σ, Höhengewicht Toleranz tol (Abbruch bei 3·tol)
function gauss(z,nx,ny,res,sg,tol){const o=new Float32Array(z.length),R=Math.max(1,Math.ceil(2.5*sg/res));
 const U=[],V=[],W=[];for(let v=-R;v<=R;v++)for(let u=-R;u<=R;u++){const dd=(u*u+v*v)*res*res;if(dd<=(2.5*sg)**2){U.push(u);V.push(v);W.push(Math.exp(-dd/(2*sg*sg)));}}
 const nk=U.length,OF=new Int32Array(nk),WD=Float64Array.from(W);for(let q=0;q<nk;q++)OF[q]=V[q]*nx+U[q];
 const LN=4096,rmax=4.5,LUT=new Float64Array(LN+2),is=LN/rmax,it2=1/(2*tol*tol);   // exp(-dz²/2tol²) als Tabelle
 for(let k=0;k<=LN+1;k++)LUT[k]=Math.exp(-k/is);
 for(let j=0;j<ny;j++){const inY=j>=R&&j<ny-R;for(let i=0;i<nx;i++){const k0=j*nx+i,c=z[k0];if(c!==c){o[k0]=NaN;continue;}let s=0,w=0;
  if(inY&&i>=R&&i<nx-R){for(let q=0;q<nk;q++){const h=z[k0+OF[q]];if(h!==h)continue;const r=(h-c)*(h-c)*it2;if(r>rmax)continue;const ww=WD[q]*LUT[(r*is)|0];s+=ww*h;w+=ww;}}
  else{for(let q=0;q<nk;q++){const u=i+U[q],v=j+V[q];if(u<0||v<0||u>=nx||v>=ny)continue;const h=z[v*nx+u];if(h!==h)continue;const r=(h-c)*(h-c)*it2;if(r>rmax)continue;const ww=WD[q]*LUT[(r*is)|0];s+=ww*h;w+=ww;}}
  o[k0]=s/w;}}
 return o;}
// Ebene Flächen erkennen (Region Growing mit laufendem Ebenenfit) und exakt auf die Ebene setzen
function flatten(z,nx,ny,res,tol,minA){const n=nx*ny,lab=new Uint8Array(n),out=Float32Array.from(z),q=new Int32Array(n),minN=Math.max(6,minA/(res*res));
 for(let s0=0;s0<n;s0++){if(lab[s0]||!isFinite(z[s0]))continue;
  let S=[0,0,0,0,0,0,0,0,0];   // n,Sx,Sy,Sz,Sxx,Sxy,Syy,Sxz,Syz
  const add=k=>{const x=(k%nx)*res,y=Math.floor(k/nx)*res,v=z[k];S[0]++;S[1]+=x;S[2]+=y;S[3]+=v;S[4]+=x*x;S[5]+=x*y;S[6]+=y*y;S[7]+=x*v;S[8]+=y*v;};
  let P=[0,0,z[s0]],head=0,tail=0;q[tail++]=s0;lab[s0]=1;add(s0);
  while(head<tail){const c=q[head++],ci=c%nx,cj=(c-ci)/nx;
   for(const[di,dj]of[[1,0],[-1,0],[0,1],[0,-1]]){const i2=ci+di,j2=cj+dj;if(i2<0||j2<0||i2>=nx||j2>=ny)continue;const k=j2*nx+i2;
    if(lab[k]||!isFinite(z[k]))continue;if(Math.abs(P[0]*i2*res+P[1]*j2*res+P[2]-z[k])>=tol)continue;
    lab[k]=1;q[tail++]=k;add(k);if(S[0]>=6)P=solve3(S)||P;}}
  if(tail<minN)continue;
  let e2=0;for(let t=0;t<tail;t++){const k=q[t],r=P[0]*(k%nx)*res+P[1]*Math.floor(k/nx)*res+P[2]-z[k];e2+=r*r;}
  if(Math.sqrt(e2/tail)>tol*0.5)continue;          // gekrümmte Fläche -> nicht begradigen
  for(let t=0;t<tail;t++){const k=q[t],pz=P[0]*(k%nx)*res+P[1]*Math.floor(k/nx)*res+P[2];if(Math.abs(pz-z[k])<tol)out[k]=pz;}}
 return out;}
function solve3(S){const[n,sx,sy,sz,xx,xy,yy,xz,yz]=S,A=[[xx,xy,sx],[xy,yy,sy],[sx,sy,n]],b=[xz,yz,sz];
 const det=M=>M[0][0]*(M[1][1]*M[2][2]-M[1][2]*M[2][1])-M[0][1]*(M[1][0]*M[2][2]-M[1][2]*M[2][0])+M[0][2]*(M[1][0]*M[2][1]-M[1][1]*M[2][0]);
 const D=det(A);if(Math.abs(D)<1e-9)return null;const r=[];
 for(let c=0;c<3;c++){const M=A.map((row,i)=>row.map((v,j)=>j===c?b[i]:v));r.push(det(M)/D);}return r;}

// ---------- Umriss: Marching Squares (Kante subpixelgenau auf Höhe = Mindesthöhe), glätten ----------
let cCache=null;
function contours(){const H=heights(),key=hCache.key+'|'+$('cs').value+'|'+$('cma').value;if(cCache&&cCache.key===key)return cCache.C;
 const{nx,ny,res}=grid,zr=H.zr,thr=H.thr;
 const F=(i,j)=>{if(i<0||j<0||i>=nx||j>=ny)return -thr;const v=zr[j*nx+i];return isFinite(v)?v-thr:-thr;};
 const pts=new Map(),adj=new Map();
 const ep=(key,x1,y1,f1,x2,y2,f2)=>{if(!pts.has(key)){const t=f1/(f1-f2);pts.set(key,[(x1+t*(x2-x1))*res,(y1+t*(y2-y1))*res]);}return key;};
 const seg=(a,b)=>{(adj.get(a)||adj.set(a,[]).get(a)).push(b);(adj.get(b)||adj.set(b,[]).get(b)).push(a);};
 for(let j=-1;j<ny;j++)for(let i=-1;i<nx;i++){
  const fa=F(i,j),fb=F(i+1,j),fc=F(i+1,j+1),fd=F(i,j+1),c=(fa>0)|(fb>0)<<1|(fc>0)<<2|(fd>0)<<3;if(c===0||c===15)continue;
  const E=[()=>ep('h'+i+'_'+j,i,j,fa,i+1,j,fb),()=>ep('v'+(i+1)+'_'+j,i+1,j,fb,i+1,j+1,fc),
           ()=>ep('h'+i+'_'+(j+1),i,j+1,fd,i+1,j+1,fc),()=>ep('v'+i+'_'+j,i,j,fa,i,j+1,fd)];
  const S=(p,q)=>seg(E[p](),E[q]());const ctr=(fa+fb+fc+fd)/4>0;
  switch(c){case 1:case 14:S(3,0);break;case 2:case 13:S(0,1);break;case 3:case 12:S(3,1);break;case 4:case 11:S(1,2);break;
   case 6:case 9:S(0,2);break;case 7:case 8:S(2,3);break;
   case 5:if(ctr){S(0,1);S(2,3);}else{S(3,0);S(1,2);}break;
   case 10:if(ctr){S(3,0);S(1,2);}else{S(0,1);S(2,3);}break;}}
 // Segmente zu geschlossenen Linien verketten
 const used=new Set(),loops=[];
 for(const[k0]of adj){if(used.has(k0))continue;const L=[];let prev=null,cur=k0;
  while(cur!==undefined&&!used.has(cur)){used.add(cur);L.push(pts.get(cur));const nb=adj.get(cur);const nx2=nb[0]!==prev?nb[0]:nb[1];prev=cur;cur=nx2;}
  if(L.length>=3)loops.push(L);}
 const sg=Math.max(0,+$('cs').value),minA=Math.max(0,+$('cma').value),out=[];
 for(let L of loops){if(sg>0)L=smoothLoop(L,sg,res);L=simplifyLoop(L,Math.min(0.005,res*0.02));const A=area(L);if(Math.abs(A)<minA||L.length<3)continue;out.push({pts:L,area:A});}
 for(const o of out){let d=0;for(const p of out)if(p!==o&&inPoly(o.pts[0],p.pts))d++;o.hole=d%2===1;}
 const C={loops:out};cCache={key,C};return C;}
const area=L=>{let a=0;for(let k=0;k<L.length;k++){const[p,q]=[L[k],L[(k+1)%L.length]];a+=p[0]*q[1]-q[0]*p[1];}return a/2;};
function inPoly([x,y],P){let c=false;for(let i=0,j=P.length-1;i<P.length;j=i++){const[xi,yi]=P[i],[xj,yj]=P[j];if((yi>y)!==(yj>y)&&x<(xj-xi)*(y-yi)/(yj-yi)+xi)c=!c;}return c;}
// gleichmäßig neu abtasten, zyklisch Gauß-filtern
function smoothLoop(L,sg,res){const n=L.length,cum=[0];for(let k=0;k<n;k++){const[a,b]=[L[k],L[(k+1)%n]];cum.push(cum[k]+Math.hypot(b[0]-a[0],b[1]-a[1]));}
 const len=cum[n],N=Math.max(12,Math.round(len/Math.min(res/2,sg/2))),ds=len/N,R=[];let s=0;
 for(let m=0;m<N;m++){const t=m*ds;while(cum[s+1]<t)s++;const f=(t-cum[s])/((cum[s+1]-cum[s])||1),a=L[s],b=L[(s+1)%n];R.push([a[0]+f*(b[0]-a[0]),a[1]+f*(b[1]-a[1])]);}
 const ss=sg/ds,W=Math.min(Math.floor(N/2)-1,Math.ceil(3*ss)),w=[];for(let k=-W;k<=W;k++)w.push(Math.exp(-k*k/(2*ss*ss)));
 const ws=w.reduce((a,b)=>a+b,0);
 return R.map((_,m)=>{let x=0,y=0;for(let k=-W;k<=W;k++){const p=R[(m+k+N)%N],c=w[k+W];x+=c*p[0];y+=c*p[1];}return[x/ws,y/ws];});}
// Douglas-Peucker für geschlossene Linien
function simplifyLoop(L,tol){if(L.length<8)return L;let f=0,dm=0;for(let k=1;k<L.length;k++){const d=Math.hypot(L[k][0]-L[0][0],L[k][1]-L[0][1]);if(d>dm){dm=d;f=k;}}
 const dp=P=>{if(P.length<3)return P;const[a,b]=[P[0],P[P.length-1]],dx=b[0]-a[0],dy=b[1]-a[1],l=Math.hypot(dx,dy)||1;let im=0,m=0;
  for(let k=1;k<P.length-1;k++){const d=Math.abs(dy*(P[k][0]-a[0])-dx*(P[k][1]-a[1]))/l;if(d>m){m=d;im=k;}}
  if(m<=tol)return[a,b];const l1=dp(P.slice(0,im+1)),l2=dp(P.slice(im));return l1.slice(0,-1).concat(l2);};
 const A=dp(L.slice(0,f+1)),B=dp(L.slice(f).concat([L[0]]));return A.slice(0,-1).concat(B.slice(0,-1));}
function makeDxf(C){const o=['0','SECTION','2','HEADER','9','$ACADVER','1','AC1009','9','$INSUNITS','70','4','0','ENDSEC','0','SECTION','2','ENTITIES'];
 for(const L of C.loops){const ly=L.hole?'LOCH':'AUSSEN';o.push('0','POLYLINE','8',ly,'66','1','70','1','10','0.0','20','0.0','30','0.0');
  for(const[x,y]of L.pts)o.push('0','VERTEX','8',ly,'10',x.toFixed(4),'20',y.toFixed(4),'30','0.0');o.push('0','SEQEND','8',ly);}
 o.push('0','ENDSEC','0','EOF');return new Blob([o.join('\r\n')+'\r\n'],{type:'application/dxf'});}
function makeSvg(C){let x0=Infinity,y0=Infinity,x1=-Infinity,y1=-Infinity;for(const L of C.loops)for(const[x,y]of L.pts){x0=Math.min(x0,x);y0=Math.min(y0,y);x1=Math.max(x1,x);y1=Math.max(y1,y);}
 const m=1,w=x1-x0+2*m,h=y1-y0+2*m;
 const d=C.loops.map(L=>'M'+L.pts.map(([x,y])=>x.toFixed(4)+' '+y.toFixed(4)).join(' L')+' Z').join(' ');
 const s=`<?xml version="1.0" encoding="UTF-8"?>\n<svg xmlns="http://www.w3.org/2000/svg" width="${w.toFixed(3)}mm" height="${h.toFixed(3)}mm" viewBox="${(x0-m).toFixed(4)} ${(-y1-m).toFixed(4)} ${w.toFixed(4)} ${h.toFixed(4)}">\n<g transform="scale(1,-1)"><path d="${d}" fill="#cfd8e6" fill-rule="evenodd" stroke="#000" stroke-width="0.1"/></g>\n</svg>\n`;
 return new Blob([s],{type:'image/svg+xml'});}
// Kantenerhaltender Mittelwert: nur Nachbarn mit |Δz| <= tol werden gemittelt
function smooth(z,nx,ny,tol,r){const o=new Float32Array(z.length);
 for(let j=0;j<ny;j++)for(let i=0;i<nx;i++){const c=z[j*nx+i];if(!isFinite(c)){o[j*nx+i]=NaN;continue;}let s=0,n=0;
  for(let v=Math.max(0,j-r);v<=Math.min(ny-1,j+r);v++)for(let u=Math.max(0,i-r);u<=Math.min(nx-1,i+r);u++){const w=z[v*nx+u];if(isFinite(w)&&Math.abs(w-c)<=tol){s+=w;n++;}}
  o[j*nx+i]=s/n;}
 return o;}

// ---------- Netz (geschlossener Körper: Oberfläche + Wände + Boden auf Z=0) ----------
function buildMesh(){const H=heights(),G=grid,k=Math.max(1,Math.round(+$('dec').value)||1);
 const nx=Math.floor((G.nx-1)/k)+1,ny=Math.floor((G.ny-1)/k)+1,r=G.res*k;
 const Z=(i,j)=>H.z[(j*k)*G.nx+i*k],cx=nx-1,cy=ny-1;
 if(cx<1||cy<1)return{V:new Float64Array(0),faces:[],H};
 const cell=new Uint8Array(cx*cy);
 for(let j=0;j<cy;j++)for(let i=0;i<cx;i++)cell[j*cx+i]=(isFinite(Z(i,j))&&isFinite(Z(i+1,j))&&isFinite(Z(i,j+1))&&isFinite(Z(i+1,j+1)))?1:0;
 const C=(i,j)=>i>=0&&j>=0&&i<cx&&j<cy&&cell[j*cx+i]===1;
 for(let ch=true;ch;){ch=false;for(let j=1;j<cy;j++)for(let i=1;i<cx;i++){const a=C(i-1,j-1),b=C(i,j-1),c=C(i-1,j),e=C(i,j);
  if(a&&e&&!b&&!c){cell[j*cx+i]=0;ch=true;}else if(b&&c&&!a&&!e){cell[j*cx+i-1]=0;ch=true;}}}  // nur-diagonal verbundene Zellen trennen (manifold)
 const V=[],top=new Int32Array(nx*ny).fill(-1),bot=new Int32Array(nx*ny).fill(-1);
 const vt=(i,j)=>{const q=j*nx+i;if(top[q]<0){top[q]=V.length/3;V.push(i*r,j*r,Z(i,j));}return top[q];};
 const vb=(i,j)=>{const q=j*nx+i;if(bot[q]<0){bot[q]=V.length/3;V.push(i*r,j*r,0);}return bot[q];};
 const F=[];
 for(let j=0;j<cy;j++)for(let i=0;i<cx;i++){if(!C(i,j))continue;
  const p00=vt(i,j),p10=vt(i+1,j),p11=vt(i+1,j+1),p01=vt(i,j+1),b00=vb(i,j),b10=vb(i+1,j),b11=vb(i+1,j+1),b01=vb(i,j+1);
  F.push([p00,p10,p11],[p00,p11,p01]);
  const bf=[b00,b01,b11,b10];bf.bottom=true;F.push(bf);
  if(!C(i,j-1))F.push([b00,b10,p10,p00]);
  if(!C(i,j+1))F.push([b11,b01,p01,p11]);
  if(!C(i-1,j))F.push([b01,b00,p00,p01]);
  if(!C(i+1,j))F.push([b10,b11,p11,p10]);}
 return{V:Float64Array.from(V),faces:F,H};}

// ---------- ZIP (store) für 3MF ----------
const CRC=(()=>{const t=new Uint32Array(256);for(let n=0;n<256;n++){let c=n;for(let k=0;k<8;k++)c=c&1?0xEDB88320^(c>>>1):c>>>1;t[n]=c>>>0;}return t;})();
function crc32(u){let c=0xFFFFFFFF;for(let i=0;i<u.length;i++)c=CRC[(c^u[i])&255]^(c>>>8);return(c^0xFFFFFFFF)>>>0;}
function zip(files){const te=new TextEncoder(),parts=[],cen=[];let off=0,csz=0;
 for(const[name,data]of files){const nb=te.encode(name),crc=crc32(data),lh=new DataView(new ArrayBuffer(30));
  lh.setUint32(0,0x04034b50,true);lh.setUint16(4,20,true);lh.setUint16(12,33,true);lh.setUint32(14,crc,true);
  lh.setUint32(18,data.length,true);lh.setUint32(22,data.length,true);lh.setUint16(26,nb.length,true);
  parts.push(lh,nb,data);
  const ch=new DataView(new ArrayBuffer(46));ch.setUint32(0,0x02014b50,true);ch.setUint16(4,20,true);ch.setUint16(6,20,true);
  ch.setUint16(14,33,true);ch.setUint32(16,crc,true);ch.setUint32(20,data.length,true);ch.setUint32(24,data.length,true);
  ch.setUint16(28,nb.length,true);ch.setUint32(42,off,true);cen.push(ch,nb);
  off+=30+nb.length+data.length;csz+=46+nb.length;}
 const eo=new DataView(new ArrayBuffer(22));eo.setUint32(0,0x06054b50,true);eo.setUint16(8,files.length,true);eo.setUint16(10,files.length,true);
 eo.setUint32(12,csz,true);eo.setUint32(16,off,true);
 return new Blob([...parts,...cen,eo],{type:'model/3mf'});}
function make3mf(M){const V=M.V,o=['<?xml version="1.0" encoding="UTF-8"?>\n<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">\n<resources>\n<object id="1" name="Scan" type="model">\n<mesh>\n<vertices>\n'];
 for(let k=0;k<V.length;k+=3)o.push(`<vertex x="${V[k].toFixed(4)}" y="${V[k+1].toFixed(4)}" z="${V[k+2].toFixed(4)}"/>\n`);
 o.push('</vertices>\n<triangles>\n');
 for(const f of M.faces)for(let q=1;q+1<f.length;q++)o.push(`<triangle v1="${f[0]}" v2="${f[q]}" v3="${f[q+1]}"/>\n`);
 o.push('</triangles>\n</mesh>\n</object>\n</resources>\n<build>\n<item objectid="1"/>\n</build>\n</model>\n');
 const te=new TextEncoder();
 const CT='<?xml version="1.0" encoding="UTF-8"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/></Types>';
 const RL='<?xml version="1.0" encoding="UTF-8"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Target="/3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>';
 return zip([['[Content_Types].xml',te.encode(CT)],['_rels/.rels',te.encode(RL)],['3D/3dmodel.model',te.encode(o.join(''))]]);}

// ---------- STL (binär) ----------
function makeStl(M){const V=M.V;let nt=0;for(const f of M.faces)nt+=f.length-2;
 const buf=new ArrayBuffer(84+50*nt),dv=new DataView(buf);dv.setUint32(80,nt,true);let o=84;
 for(const f of M.faces)for(let q=1;q+1<f.length;q++){const A=f[0]*3,B=f[q]*3,C=f[q+1]*3;
  const ux=V[B]-V[A],uy=V[B+1]-V[A+1],uz=V[B+2]-V[A+2],vx=V[C]-V[A],vy=V[C+1]-V[A+1],vz=V[C+2]-V[A+2];
  let nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;const n=Math.hypot(nx,ny,nz)||1;
  for(const w of[nx/n,ny/n,nz/n])dv.setFloat32(o,w,true),o+=4;
  for(const P of[A,B,C])for(let c=0;c<3;c++)dv.setFloat32(o,V[P+c],true),o+=4;
  o+=2;}
 return new Blob([buf],{type:'model/stl'});}

// ---------- STEP AP214 (Volumenkörper, planare Flächen) ----------
function makeStep(M,name){const V=M.V,L=[];let id=0;const E=s=>{L.push('#'+(++id)+'='+s+';');return id;};
 const r=v=>(Math.abs(v)<5e-6?0:v).toFixed(5),r8=v=>(Math.abs(v)<1e-12?0:v).toFixed(10);
 const dC=new Map(),dir=(x,y,z)=>{const n=Math.hypot(x,y,z);const k=`${r8(x/n)},${r8(y/n)},${r8(z/n)}`;let e=dC.get(k);if(!e){e=E(`DIRECTION('',(${k}))`);dC.set(k,e);}return e;};
 const app=E("APPLICATION_CONTEXT('core data for automotive mechanical design processes')");
 E(`APPLICATION_PROTOCOL_DEFINITION('international standard','automotive_design',2000,#${app})`);
 const mm=E('(LENGTH_UNIT()NAMED_UNIT(*)SI_UNIT(.MILLI.,.METRE.))');
 const rad=E('(NAMED_UNIT(*)PLANE_ANGLE_UNIT()SI_UNIT($,.RADIAN.))');
 const sr=E('(NAMED_UNIT(*)SI_UNIT($,.STERADIAN.)SOLID_ANGLE_UNIT())');
 const unc=E(`UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(1.E-05),#${mm},'distance_accuracy_value','confusion accuracy')`);
 const ctx=E(`(GEOMETRIC_REPRESENTATION_CONTEXT(3)GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#${unc}))GLOBAL_UNIT_ASSIGNED_CONTEXT((#${mm},#${rad},#${sr}))REPRESENTATION_CONTEXT('Context','3D'))`);
 const org=E("CARTESIAN_POINT('',(0.,0.,0.))"),xAx=dir(1,0,0);
 const wcs=E(`AXIS2_PLACEMENT_3D('',#${org},#${dir(0,0,1)},#${xAx})`);
 const nv=V.length/3,cp=new Int32Array(nv),vp=new Int32Array(nv);
 for(let v=0;v<nv;v++){cp[v]=E(`CARTESIAN_POINT('',(${r(V[3*v])},${r(V[3*v+1])},${r(V[3*v+2])}))`);vp[v]=E(`VERTEX_POINT('',#${cp[v]})`);}
 const edges=new Map();
 const oe=(a,b)=>{const lo=Math.min(a,b),hi=Math.max(a,b),key=lo*nv+hi;let e=edges.get(key);
  if(e===undefined){const dx=V[3*hi]-V[3*lo],dy=V[3*hi+1]-V[3*lo+1],dz=V[3*hi+2]-V[3*lo+2];
   const vec=E(`VECTOR('',#${dir(dx,dy,dz)},${r(Math.hypot(dx,dy,dz))})`),ln=E(`LINE('',#${cp[lo]},#${vec})`);
   e=E(`EDGE_CURVE('',#${vp[lo]},#${vp[hi]},#${ln},.T.)`);edges.set(key,e);}
  return E(`ORIENTED_EDGE('',*,*,#${e},${a===lo?'.T.':'.F.'})`);};
 let botPl=0;const fids=[];
 for(const f of M.faces){let pl;
  if(f.bottom){if(!botPl)botPl=E(`PLANE('',#${E(`AXIS2_PLACEMENT_3D('',#${org},#${dir(0,0,-1)},#${xAx})`)})`);pl=botPl;}
  else{let nx=0,ny=0,nz=0;for(let q=0;q<f.length;q++){const a=f[q]*3,b=f[(q+1)%f.length]*3;
    nx+=(V[a+1]-V[b+1])*(V[a+2]+V[b+2]);ny+=(V[a+2]-V[b+2])*(V[a]+V[b]);nz+=(V[a]-V[b])*(V[a+1]+V[b+1]);}
   const a=f[0]*3,b=f[1]*3;
   const ax=E(`AXIS2_PLACEMENT_3D('',#${cp[f[0]]},#${dir(nx,ny,nz)},#${dir(V[b]-V[a],V[b+1]-V[a+1],V[b+2]-V[a+2])})`);
   pl=E(`PLANE('',#${ax})`);}
  const oes=f.map((v,q)=>'#'+oe(v,f[(q+1)%f.length]));
  const lp=E(`EDGE_LOOP('',(${oes.join(',')}))`),fb=E(`FACE_OUTER_BOUND('',#${lp},.T.)`);
  fids.push('#'+E(`ADVANCED_FACE('',(#${fb}),#${pl},.T.)`));}
 const sh=E(`CLOSED_SHELL('',(${fids.join(',')}))`),msb=E(`MANIFOLD_SOLID_BREP('${name}',#${sh})`);
 const rep=E(`ADVANCED_BREP_SHAPE_REPRESENTATION('${name}',(#${wcs},#${msb}),#${ctx})`);
 const pc=E(`PRODUCT_CONTEXT('',#${app},'mechanical')`),pr=E(`PRODUCT('${name}','${name}','',(#${pc}))`);
 E(`PRODUCT_RELATED_PRODUCT_CATEGORY('part','',(#${pr}))`);
 const pdf=E(`PRODUCT_DEFINITION_FORMATION('','',#${pr})`),pdc=E(`PRODUCT_DEFINITION_CONTEXT('part definition',#${app},'design')`);
 const pd=E(`PRODUCT_DEFINITION('design','',#${pdf},#${pdc})`),pds=E(`PRODUCT_DEFINITION_SHAPE('','',#${pd})`);
 E(`SHAPE_DEFINITION_REPRESENTATION(#${pds},#${rep})`);
 const head=`ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION(('CoreXY Laser Scan'),'2;1');\nFILE_NAME('${name}.step','${new Date().toISOString().slice(0,19)}',(''),(''),'CoreXY-Scanner','CoreXY-Scanner','');\nFILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));\nENDSEC;\nDATA;\n`;
 return[head,L.join('\n'),'\nENDSEC;\nEND-ISO-10303-21;\n'];}

// ---------- Export ----------
function dl(blob,name){const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download=name;document.body.appendChild(a);a.click();
 setTimeout(()=>{URL.revokeObjectURL(a.href);a.remove();},2000);}
function info(H,M){let s=(H.nHole||H.nOut?`<span>Fehlstellen</span><span>${H.nHole} Loch/Löcher geschlossen, ${H.nOut} Ausreißer entfernt</span>`:'')+`<span>Z-Null-Ebene</span><span>d = ${fx(H.P[0],5)}·x + ${fx(H.P[1],5)}·y + ${fx(H.P[2])} mm</span><span>Teilpunkte</span><span>${H.cnt}</span>`;
 if(M)s+=`<span>Netz</span><span>${M.V.length/3} Vertices, ${M.faces.length} Flächen</span>`;$('einfo').innerHTML=s;}
function exp(t){try{if(!grid)throw new Error('Keine Scandaten');if(st.state==='SCAN'||st.state==='PAUSED')throw new Error('Scan läuft noch');
 const fn=($('fn').value||'scan').replace(/[^\w\-]/g,'_');
 if(t==='dxf'||t==='svg'){const C=contours();if(!C.loops.length)throw new Error('Kein Umriss gefunden (Mindesthöhe prüfen)');
  dl(t==='dxf'?makeDxf(C):makeSvg(C),fn+'.'+t);const nh=C.loops.filter(L=>L.hole).length;
  $('einfo').innerHTML=`<span>Umriss</span><span>${C.loops.length-nh} Außenkontur(en), ${nh} Loch/Löcher</span>`;msg('');return;}
 if(t==='csv'){const H=heights(),{nx,ny,res}=grid,L=['x_mm,y_mm,z_mm'];
  const tz=+$('dz').value,del=tz>0&&$('dzm').value==='del';
  for(let j=0;j<ny;j++){let zk=NaN;for(let i=0;i<nx;i++){const z=H.z[j*nx+i];if(!isFinite(z)){zk=NaN;continue;}
   if(del&&isFinite(zk)){const zn=i+1<nx?H.z[j*nx+i+1]:NaN;   // Lauf-Ende (Stufe/Rand) immer behalten
    if(Math.abs(z-zk)<=tz&&isFinite(zn)&&Math.abs(zn-zk)<=tz)continue;}
   zk=z;L.push(`${(i*res).toFixed(3)},${(j*res).toFixed(3)},${z.toFixed(3)}`);}}
  dl(new Blob([L.join('\n')+'\n'],{type:'text/csv'}),fn+'.csv');info(H);msg('');return;}
 const M=buildMesh();if(!M.faces.length)throw new Error('Kein zusammenhängender Teilbereich (mind. 2×2 Punkte über Schwelle nötig)');
 if(t==='3mf')dl(make3mf(M),fn+'.3mf');
 else if(t==='stl')dl(makeStl(M),fn+'.stl');
 else{if(M.faces.length>300000&&!confirm(M.faces.length+' Flächen – STEP wird sehr groß. Ausdünnung erhöhen? (Abbrechen = ja)'))return;
  dl(new Blob(makeStep(M,fn),{type:'application/step'}),fn+'.step');}
 info(M.H,M);msg('');}catch(e){msg(e.message||e);}}
function ota(){const f=$('otaf').files[0];if(!f){$('otat').textContent='Keine Datei gewählt';return;}
 if(!/\.bin$/i.test(f.name)&&!confirm('Keine .bin-Datei – trotzdem flashen?'))return;
 if((st.state==='SCAN'||st.state==='RETURN'||st.state==='PAUSED')&&!confirm('Scan läuft – abbrechen und flashen?'))return;
 const fd=new FormData();fd.append('firmware',f,f.name);const x=new XMLHttpRequest();
 x.upload.onprogress=e=>{if(e.lengthComputable){$('otap').max=e.total;$('otap').value=e.loaded;
  $('otat').textContent=Math.round(100*e.loaded/e.total)+' %  ('+(e.loaded>>10)+' / '+(e.total>>10)+' kB)';}};
 x.onload=()=>{if(x.status===200){$('otat').textContent='Fertig – ESP startet neu, Seite lädt in 10 s neu …';setTimeout(()=>location.reload(),10000);}
  else $('otat').textContent='Fehler: '+x.responseText;};
 x.onerror=()=>{$('otat').textContent='Übertragungsfehler';};
 $('otap').value=0;$('otat').textContent='Übertrage '+f.name+' …';x.open('POST','/update');x.send(fd);}
document.querySelectorAll('details').forEach(d=>{try{const v=localStorage.getItem(d.id);if(v!==null)d.open=v==='1';}catch(e){}
 d.addEventListener('toggle',()=>{try{localStorage.setItem(d.id,d.open?'1':'0');}catch(e){}});});
updZ();
poll();
</script></body></html>
)rawliteral";
