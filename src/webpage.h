#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CoreXY Scanner</title>
<style>
body{font-family:system-ui,sans-serif;margin:0;background:#15181c;color:#e6e6e6}
main{max-width:1500px;margin:auto;padding:12px;display:grid;gap:12px;grid-template-columns:repeat(3,minmax(0,1fr));align-items:start}
.col{display:flex;flex-direction:column;gap:12px;min-width:0}#s-prev{grid-column:1/-1}
.cgrid{display:grid;grid-template-columns:1fr;gap:12px}#ccv{width:100%;border-radius:4px}
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
#tip{position:fixed;z-index:50;max-width:330px;background:#0f1215;color:#e6e6e6;border:1px solid #2d6cdf;border-radius:6px;padding:7px 10px;font-size:12.5px;line-height:1.4;box-shadow:0 4px 16px rgba(0,0,0,.55);pointer-events:none;display:none}
[data-tip]{cursor:help}
details.sub{background:#181c21;border:1px solid #2a313a;border-radius:6px;padding:4px 10px;margin:8px 0}
details.sub summary{font-size:14px;color:#8fc1ff;padding:3px 0}
.jb{touch-action:none;user-select:none;-webkit-user-select:none}
#msg{color:#ffb347;min-height:1.2em;font-size:13px}.wide{grid-column:1/-1}
</style></head><body>
<div class="bar"><b>CoreXY Scanner</b><span id="bstate">-</span><span id="bpos">-</span><button id="bpause" class="gr" onclick="cmd('/api/pause')" style="margin-left:auto">PAUSE</button><button class="red" style="margin-left:0" onclick="cmd('/api/stop')">STOP</button></div>
<main>
<div class="col">
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
<details id="d-man"><summary>Manuell verfahren</summary>
<label>Schrittweite mm<select id="js"><option>0.1</option><option>1</option><option selected>10</option><option>50</option></select></label>
<label>Geschwindigkeit mm/s<input id="jf" type="number" value="30"></label>
<div class="jog"><span></span><button class="jb" data-dx="0" data-dy="1">Y+</button><span></span>
<button class="jb" data-dx="-1" data-dy="0">X−</button><span></span><button class="jb" data-dx="1" data-dy="0">X+</button>
<span></span><button class="jb" data-dx="0" data-dy="-1">Y−</button><span></span></div>
<div style="font-size:12px;color:#8fa0b8;text-align:center;margin-top:-4px">kurz tippen = ein Schritt · gedrückt halten = fahren</div>
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
</div>
<div class="col">
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
<label>Höhen-Offset mm (Unterlage abziehen)<input id="zoff" type="number" value="0" step="any" onchange="draw()"></label>
<label>Kontur als Rand verwenden (scharfe Kanten)<input id="cmask" type="checkbox" onchange="draw()"></label>
<details id="d-ex-fs" class="sub"><summary>Fehlstellen</summary>
<label>Kleine Löcher schließen<input id="hf" type="checkbox" checked onchange="draw()"></label>
<label>max. Lochfläche mm² (größer = echtes Loch)<input id="hfa" type="number" value="4" step="any" onchange="draw()"></label>
<label>Kantenkorrektur bis mm nach innen (nur Kontur als Rand)<input id="ebw" type="number" value="2" step="any" min="0" onchange="draw()"></label>
<label>Randstreifen neu berechnen mm (nur Kontur als Rand)<input id="eb" type="number" value="0" step="any" onchange="draw()"></label>
<label>Ausreißer entfernen ab mm (0 = aus)<input id="ol" type="number" value="0" step="any" onchange="draw()"></label>
</details>
<details id="d-ex-gl" class="sub"><summary>Oberfläche glätten</summary>
<label>Modus<select id="dzm" onchange="draw()"><option value="gauss">Gauß, kantenerhaltend (alle Exporte)</option><option value="avg">Mittelwert (alle Exporte)</option><option value="del">Löschen (nur CSV)</option></select></label>
<label>Z-Toleranz mm (0 = aus, Kanten > 3× bleiben)<input id="dz" type="number" value="0" step="any" onchange="draw()"></label>
<label>Gauß-Radius σ mm<input id="gs" type="number" value="0.5" step="any" onchange="draw()"></label>
<label>Gauß-Durchläufe<input id="gp" type="number" value="1" min="1" onchange="draw()"></label>
<label>Mittelwert-Radius (Punkte)<input id="dzr" type="number" value="1" min="1" onchange="draw()"></label>
<label>Ebene Flächen begradigen<input id="fl" type="checkbox" onchange="draw()"></label>
<label>Ebenen-Toleranz mm<input id="flt" type="number" value="0.05" step="any" onchange="draw()"></label>
<label>Ebene min. Fläche mm²<input id="fla" type="number" value="25" step="any" onchange="draw()"></label>
</details>
<details id="d-ex-hn" class="sub"><summary>Höhe normieren</summary>
<label>Wölbung entfernen, Radius mm (0 = aus)<input id="nwr" type="number" value="0" step="any" min="0" onchange="draw()"></label>
<label>Max. Höhe mm (0 = aus)<input id="nmx" type="number" value="0" step="any" min="0" onchange="draw()"></label>
<label>Grunddicke mm<input id="ngd" type="number" value="0" step="any" min="0" onchange="draw()"></label>
</details>
<details id="d-ex-um" class="sub"><summary>Umriss</summary>
<label>Umriss-Glättung σ mm (0 = aus)<input id="cs" type="number" value="0.5" step="any" onchange="draw()"></label>
<label>Konturen kleiner als mm² ignorieren<input id="cma" type="number" value="2" step="any" onchange="draw()"></label>
<label>Umriss in Vorschau zeigen<input id="cpv" type="checkbox" onchange="draw()"></label>
</details>
<details id="d-ex-da" class="sub" open><summary>Dateien</summary>
<label>Schalendicke mm (0 = Unterseite eben)<input id="sht" type="number" value="0" step="any" min="0"></label>
<label>Ausdünnung 3MF/STL/STEP (jeder k-te Punkt)<input id="dec" type="number" value="1" min="1"></label>
<label>STEP max. Flächen (Ausdünnung automatisch)<input id="smax" type="number" value="50000" step="1000"></label>
<label>Dateiname<input id="fn" value="scan"></label>
<button onclick="exp('csv')">CSV</button><button onclick="exp('3mf')">3MF</button><button onclick="exp('stl')">STL</button><button onclick="exp('step')">STEP</button><button onclick="exp('dxf')">Umriss DXF</button><button onclick="exp('svg')">Umriss SVG</button>
<div class="kv" style="margin-top:8px"><span>Scan-Datei (.cxs)</span><span>Höhendaten + Konturen</span></div>
<button onclick="cxsSave()">Scan → PC speichern</button><button onclick="$('cxsf').click()">PC → ESP laden</button>
<input id="cxsf" type="file" accept=".cxs" style="display:none" onchange="cxsLoad(this.files[0]);this.value=''">
<progress id="cxsp" value="0" max="1" style="display:none"></progress><div id="cxst" class="kv"></div>
</details>
<div id="einfo" class="kv"></div>
</details>
</div>
<div class="col">
<details id="d-cont"><summary>Kontur abfahren (nur Umriss, ohne Flächenscan)</summary>
<div class="cgrid"><div>
<label>Suchrichtung<select id="cdir"><option value="0">+X (Teil liegt rechts)</option><option value="2">−X (Teil liegt links)</option><option value="1">+Y (Teil liegt hinten)</option><option value="3">−Y (Teil liegt vorne)</option></select></label>
<label>Schrittweite mm<input id="cstep" type="number" value="0.5" step="any"></label>
<label>Kantenhöhe mm<input id="cthr" type="number" value="0.5" step="any"></label>
<label>Verfeinerung (Halbierungen)<input id="cref" type="number" value="2" min="0" max="6"></label>
<label>Max. Suchweg / Ausdehnung mm<input id="cmax" type="number" value="300"></label>
<label>Geschwindigkeit mm/s<input id="cf" type="number" value="30"></label>
<label>Beruhigung ms<input id="cset" type="number" value="30"></label>
<label>Mittelung (Samples)<input id="cavg" type="number" value="16"></label>
<label>Glättung σ mm (0 = aus)<input id="csm" type="number" value="0.3" step="any" onchange="drawC()"></label>
<button onclick="cStart(0)">Außenkontur abfahren</button><button onclick="cStart(1)">Innenkontur abfahren</button><br>
<button onclick="cScanInside()" title="Höhenscan nur innerhalb der Konturen, mit den Scan-Einstellungen (Auflösung, Messart …)">Fläche innerhalb scannen</button><br>
<button class="gr" onclick="cUndo()">Letzte entfernen</button><button class="gr" onclick="cClear()">Alle löschen</button>
<button onclick="cExp('dxf')">DXF</button><button onclick="cExp('svg')">SVG</button><button onclick="$('dxff').click()" title="Bearbeiteten Umriss (DXF) wieder als Konturen übernehmen">DXF importieren</button>
<input id="dxff" type="file" accept=".dxf" style="display:none" onchange="cImport(this.files[0]);this.value=''">
<div id="cinfo" class="kv"></div>
</div><div><canvas id="ccv" width="10" height="10"></canvas></div></div>
</details>
</div>
<section id="s-prev"><h2>Vorschau</h2>
<label style="justify-content:flex-start">Nur Teil anzeigen (Z-Null/Schwelle aus Export)<input id="pv" type="checkbox" onchange="draw()"></label>
<canvas id="cv" width="10" height="10"></canvas><div id="leg" class="kv"></div>
<h2 style="margin-top:10px">Maße</h2><div id="meas" class="kv"></div>
<button class="gr" onclick="loadData()">Daten neu laden</button>
<button class="gr" onclick="flip('x')" title="Links und rechts tauschen (X spiegeln)">↔ Horizontal spiegeln</button>
<button class="gr" onclick="flip('y')" title="Oben und unten tauschen (Y spiegeln)">↕ Vertikal spiegeln</button>
<span id="flipI" style="color:#ffb347;font-size:13px;margin-left:6px"></span>
</section>
</main>
<script>
const $=id=>document.getElementById(id);
let st={},grid=null,lastFetch=0,prevState='';
const fx=(v,n=3)=>v==null||!isFinite(v)?'–':(+v).toFixed(n);
function msg(t){$('msg').textContent=t||'';}
async function cmd(u){try{const r=await fetch(u,{method:'POST'});const t=await r.text();if(!r.ok)throw new Error(t);msg('');}catch(e){msg(e.message);}}
function jog(sx,sy){const s=+$('js').value;cmd(`/api/jog?dx=${sx*s}&dy=${sy*s}&f=${$('jf').value}`);}
function startScan(){flipX=flipY=false;const q=['w','h','res','feed','travel','settle','avg'].map(k=>k+'='+encodeURIComponent($(k).value)).join('&')+'&bidir='+($('bidir').checked?1:0)+'&fly='+($('fly').checked?1:0)+'&lat='+encodeURIComponent($('lat').value);grid=null;cmd('/api/scan?'+q);}
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
  const rx=st.x-(grid?grid.ox:0),ry=st.y-(grid?grid.oy:0);   // Live-Z: Rasterkoordinaten, ggf. gespiegelt
  const gx=grid&&flipX?(grid.nx-1)*grid.res-rx:rx,gy=grid&&flipY?(grid.ny-1)*grid.res-ry:ry;
  const zl=zP&&st.dist!=null?zP[0]*gx+zP[1]*gy+zP[2]-st.dist-Math.max(0,+$('zoff').value||0):NaN;
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
  const pb=st.state==='PAUSED'?'WEITER':'PAUSE',pe=st.state==='SCAN'||st.state==='PAUSED'||st.state==='CONTOUR';
  for(const id of['bpause','spause']){$(id).textContent=pb;$(id).disabled=!pe;$(id).style.opacity=pe?1:.4;}
  if(st.state==='SCAN'&&Date.now()-lastFetch>3000)loadData();
  if((run(prevState)&&!run(st.state))||(!grid&&st.total>0&&!run(st.state)&&Date.now()-lastFetch>5000))loadData();
  prevState=st.state;
  cPoll();cSync();
 }catch(e){$('state').textContent='keine Verbindung';}
 est();setTimeout(poll,500);}
// lädt nur neue Zeilen nach (Server liefert Zeilen ab 'from')
async function loadData(){lastFetch=Date.now();try{const nx=st.nx,ny=st.ny,res=st.res;if(!nx||!ny)return;
 const ox=st.ox||0,oy=st.oy||0;
 if(!grid||grid.nx!==nx||grid.ny!==ny||grid.res!==res||grid.ox!==ox||grid.oy!==oy||(st.rows??0)<grid.rows)grid={nx,ny,res,ox,oy,raw:new Float32Array(nx*ny).fill(NaN),d:new Float32Array(nx*ny).fill(NaN),rows:0};
 const r=await fetch('/api/data?from='+grid.rows);if(!r.ok)return;
 const from=+r.headers.get('X-Row-From'),to=+r.headers.get('X-Row-To'),q=new Int16Array(await r.arrayBuffer());
 if(q.length!==(to-from)*nx||to>ny)return;
 for(let k=0;k<q.length;k++)grid.raw[from*nx+k]=q[k]===-32768?NaN:q[k]/100;
 grid.rows=to;applyFlip();}catch(e){}}
// Spiegeln: grid.raw = Messdaten wie gescannt, grid.d = gespiegelte Ansicht für Vorschau und alle Exporte
let flipX=false,flipY=false,gridVer=0;   // gridVer global: neue Daten gleicher Größe nie mit alten Caches verwechseln
function applyFlip(){if(!grid)return;grid.ox=grid.ox||0;grid.oy=grid.oy||0;const{nx,ny,raw,d}=grid;
 for(let j=0;j<ny;j++){const js=flipY?ny-1-j:j;for(let i=0;i<nx;i++)d[j*nx+i]=raw[js*nx+(flipX?nx-1-i:i)];}
 grid.ver=++gridVer;$('flipI').textContent=(flipX||flipY)?'gespiegelt: '+[flipX?'horizontal':'',flipY?'vertikal':''].filter(Boolean).join(' + '):'';draw();}
function flip(ax){if(ax==='x')flipX=!flipX;else flipY=!flipY;applyFlip();}
function cmap(t){const c=x=>Math.max(0,Math.min(1,x))*255;return[c(1.5-Math.abs(4*t-3)),c(1.5-Math.abs(4*t-2)),c(1.5-Math.abs(4*t-1))];}
function draw(){updZ();if(!grid)return;const{nx,ny,d}=grid;let v=d,lbl='Abstand';
 if($('pv').checked){try{v=heights().z;lbl='Höhe';}catch(e){msg(e.message);}}
 let mn=Infinity,mx=-Infinity;for(const x of v)if(isFinite(x)){if(x<mn)mn=x;if(x>mx)mx=x;}
 const off=document.createElement('canvas');off.width=nx;off.height=ny;const ox=off.getContext('2d'),img=ox.createImageData(nx,ny);
 for(let j=0;j<ny;j++)for(let i=0;i<nx;i++){const val=v[j*nx+i],p=((ny-1-j)*nx+i)*4;let c=[45,45,45];
  if(isFinite(val)){let t=(val-mn)/((mx-mn)||1);if(v===d)t=1-t;c=cmap(t);}img.data[p]=c[0];img.data[p+1]=c[1];img.data[p+2]=c[2];img.data[p+3]=255;}
 ox.putImageData(img,0,0);const cv=$('cv'),sc=Math.max(1,Math.floor(900/Math.max(nx,ny)));cv.width=nx*sc;cv.height=ny*sc;
 const g=cv.getContext('2d');g.imageSmoothingEnabled=false;g.drawImage(off,0,0,cv.width,cv.height);
 if($('cpv').checked){try{const C=maskOn()?{loops:relLoops()}:contours(),r=grid.res;g.lineWidth=Math.max(1.5,sc*0.3);
  for(const L of C.loops){g.strokeStyle=L.hole?'#00e5ff':'#ffffff';g.beginPath();
   L.pts.forEach(([x,y],k)=>{const px=(x/r+0.5)*sc,py=(ny-1-y/r+0.5)*sc;k?g.lineTo(px,py):g.moveTo(px,py);});g.closePath();g.stroke();}}catch(e){msg(e.message);}}
 $('leg').innerHTML=`<span>${lbl}</span><span>${fx(mn)} … ${fx(mx)} mm  (blau = tief, rot = hoch, grau = leer)</span>`;measures();}

// ---------- Z-Null / Höhen ----------
function plane(X,Y,D,sel){let n=0,sx=0,sy=0,sd=0;for(let k=0;k<D.length;k++)if(sel[k]){n++;sx+=X[k];sy+=Y[k];sd+=D[k];}
 if(n<3)return null;const mx=sx/n,my=sy/n,md=sd/n;let xx=0,xy=0,yy=0,xd=0,yd=0;
 for(let k=0;k<D.length;k++)if(sel[k]){const x=X[k]-mx,y=Y[k]-my,dd=D[k]-md;xx+=x*x;xy+=x*y;yy+=y*y;xd+=x*dd;yd+=y*dd;}
 const det=xx*yy-xy*xy;let a=0,b=0;
 if(Math.abs(det)>1e-9*(xx*yy+1e-12)){a=(xd*yy-yd*xy)/det;b=(yd*xx-xd*xy)/det;}else if(xx>1e-12)a=xd/xx;else if(yy>1e-12)b=yd/yy;
 return[a,b,md-a*mx-b*my];}
let fbCache=null;
function fitBase(){const{nx,ny,res,d}=grid,tol=Math.max(0.01,+$('tol').value),M=maskOn()?maskGrid():null;
 const fk=grid.ver+'|'+nx+'|'+ny+'|'+tol+'|'+(M?mCache.key:'');if(fbCache&&fbCache.k===fk)return fbCache.P;
 const P=fitBase0(nx,ny,res,d,tol,M);fbCache={k:fk,P};return P;}
// Auflage-Ebene robust (RANSAC): Kandidaten = Punkte außerhalb der Kontur (M) bzw. alle.
// Gesucht wird die Ebene mit den meisten Punkten im Toleranzband, wobei Punkte DEUTLICH UNTER der Ebene
// (größerer Abstand = tiefer als die Auflage) stark bestraft werden – Teil-Oberseite und Seitenwände scheiden so aus,
// auch wenn nur ein schmaler Auflage-Rand gescannt wurde (Fläche innerhalb scannen).
function fitBase0(nx,ny,res,d,tol,M){const X=[],Y=[],D=[];
 const add=k=>{const v=d[k];if(isFinite(v)){const i=k%nx;X.push(i*res);Y.push((k-i)/nx*res);D.push(v);}};
 if(M){for(let k=0;k<d.length;k++)if(!M[k])add(k);}
 if(D.length<50){X.length=Y.length=D.length=0;for(let k=0;k<d.length;k++)add(k);}
 if(D.length<3)throw new Error('Zu wenig gültige Punkte');
 const n=D.length,step=Math.max(1,Math.floor(n/20000));let seed=12345;const rnd=()=>{seed=(seed*1103515245+12345)>>>0;return seed/4294967296;};
 const band=Math.max(tol,0.05),below=Math.max(5*tol,1.5);let best=null,bs=-Infinity;
 // Startpunkte bevorzugt aus den entferntesten Punkten ziehen (Auflage = am weitesten weg), falls die Auflage nur wenige % ausmacht
 const ord=Array.from(D.keys()).sort((p,q)=>D[q]-D[p]),pools=[0.02,0.05,0.2,1].map(f=>Math.max(3,Math.floor(n*f)));
 for(let it=0;it<400;it++){const m=pools[it&3],a=ord[Math.floor(rnd()*m)],b=ord[Math.floor(rnd()*m)],c=ord[Math.floor(rnd()*m)];
  const ux=X[b]-X[a],uy=Y[b]-Y[a],uz=D[b]-D[a],vx=X[c]-X[a],vy=Y[c]-Y[a],vz=D[c]-D[a];
  const cx=uy*vz-uz*vy,cy=uz*vx-ux*vz,cz=ux*vy-uy*vx;if(Math.abs(cz)<1e-6*(Math.hypot(ux,uy)*Math.hypot(vx,vy)+1e-9)||Math.abs(cz)<1e-9)continue;
  const p0=-cx/cz,p1=-cy/cz;if(Math.abs(p0)>0.2||Math.abs(p1)>0.2)continue;     // max. ~11° Schräglage
  const p2=D[a]-p0*X[a]-p1*Y[a];let sc=0;
  for(let k=0;k<n;k+=step){const r=D[k]-(p0*X[k]+p1*Y[k]+p2);if(Math.abs(r)<band)sc++;else if(r>below)sc-=50;}
  if(sc>bs){bs=sc;best=[p0,p1,p2];}}
 let P=best;
 if(!P){const s=Float64Array.from(D).sort();P=[0,0,s[Math.floor(0.95*(n-1))]];}
 for(let it=0;it<10;it++){const sel=D.map((v,k)=>Math.abs(P[0]*X[k]+P[1]*Y[k]+P[2]-v)<tol);const Q=plane(X,Y,D,sel);if(!Q)break;P=Q;}
 return P;}
let zP=null;  // Z-Null-Ebene für Live-Z in der Kopfleiste
function baseP(){const m=$('bm').value;if(m==='manual')return[0,0,+$('dman').value];
 if(!grid)throw new Error('Keine Scandaten');if(m==='start'){if(!isFinite(grid.raw[0]))throw new Error('Messwert am Startpunkt ungültig');return[0,0,grid.raw[0]];}
 return fitBase();}
function updZ(){try{zP=baseP();}catch(e){zP=null;}}
const HP=['bm','dman','tol','thr','zoff','ebw','hf','hfa','ol','eb','dz','dzm','dzr','gs','gp','fl','flt','fla','cmask','csm','nwr','nmx','ngd'];
let hCache=null;
function heights(){if(!grid)throw new Error('Keine Scandaten');
 const key=grid.ver+'|'+grid.nx+'|'+grid.ny+'|'+cVer+'|'+flipX+flipY+'|'+HP.map(k=>$(k).type==='checkbox'?$(k).checked:$(k).value).join('|');
 if(hCache&&hCache.key===key)return hCache.H;
 const{nx,ny,res,d}=grid,thr=Math.max(0.01,+$('thr').value);const P=baseP();
 let z=new Float32Array(nx*ny);
 for(let j=0;j<ny;j++)for(let i=0;i<nx;i++){const k=j*nx+i,dv=d[k];z[k]=isFinite(dv)?P[0]*i*res+P[1]*j*res+P[2]-dv:NaN;}
 const ol=+$('ol').value;let nOut=0,nHole=0;
 if(ol>0){const r=removeOutliers(z,nx,ny,ol);z=r.z;nOut=r.n;}
 const mk=maskOn()?maskGrid():null;
 if(mk){for(let k=0;k<z.length;k++)if(!mk[k])z[k]=NaN;
  // Kantenartefakte: innerhalb der Kontur (abgefahren auf Kantenhöhe) im Streifen "Kantenkorrektur" vom Rand
  // kann ein Punkt nicht niedriger als die Kantenhöhe sein -> Fehlmessung, wird aus den Nachbarn gefüllt
  const ebw=Math.max(0,+$('ebw').value||0);
  if(ebw>0){const B=Math.max(1,Math.round(ebw/res)),L=new Int32Array(z.length).fill(-1),Q=[];
   for(let j=0;j<ny;j++)for(let i=0;i<nx;i++){const k=j*nx+i;if(!mk[k])continue;let e=false;
    for(let v=-1;v<=1&&!e;v++)for(let u=-1;u<=1;u++){const i2=i+u,j2=j+v;if(i2<0||j2<0||i2>=nx||j2>=ny||!mk[j2*nx+i2]){e=true;break;}}
    if(e){L[k]=0;Q.push(k);}}
   for(let h=0;h<Q.length;h++){const k=Q[h];if(L[k]>=B)continue;const i=k%nx;
    for(const n of[k-1,k+1,k-nx,k+nx]){if(n<0||n>=z.length||(n===k-1&&i===0)||(n===k+1&&i===nx-1))continue;if(mk[n]&&L[n]<0){L[n]=L[k]+1;Q.push(n);}}}
   for(let k=0;k<z.length;k++)if(L[k]>=0&&L[k]<B&&z[k]<thr)z[k]=NaN;
   // halb getroffene Kante: von innen nach außen, Sprung > 1 mm nach unten gegenüber dem inneren Nachbarn = Fehlmessung
   const J=Math.max(1,2*thr),byL=[];for(let k=0;k<z.length;k++)if(L[k]>=0&&L[k]<B)(byL[L[k]]||(byL[L[k]]=[])).push(k);
   for(let l=B-1;l>=0;l--)for(const k of byL[l]||[]){if(!(z[k]===z[k]))continue;const i=k%nx,j=(k-i)/nx;let ref=-Infinity;
    // Vergleich mit Punkten weiter innen; Radius wächst, bis welche gefunden sind (spitze Ecken haben nahe keine)
    for(let R=2;R<=Math.max(4,4*B)&&ref===-Infinity;R*=2)
     for(let v=-R;v<=R;v++)for(let u=-R;u<=R;u++){const i2=i+u,j2=j+v;if(i2<0||j2<0||i2>=nx||j2>=ny)continue;const n=j2*nx+i2;
      if(mk[n]&&(L[n]<0||L[n]>l)&&z[n]===z[n])ref=Math.max(ref,z[n]);}
    if(z[k]<ref-J)z[k]=NaN;}}
  // Randstreifen: Messwerte nahe der Kontur verwerfen (Kantenartefakte), werden von innen fortgesetzt
  const eb=+$('eb').value;if(eb>0){let cur=Uint8Array.from(mk);const n=Math.max(1,Math.round(eb/res));
   for(let it=0;it<n;it++){const nx2=Uint8Array.from(cur);for(let j=0;j<ny;j++)for(let i=0;i<nx;i++){const k=j*nx+i;if(!cur[k])continue;
     for(let v=-1;v<=1&&nx2[k];v++)for(let u=-1;u<=1;u++){const i2=i+u,j2=j+v;if(i2<0||j2<0||i2>=nx||j2>=ny||!cur[j2*nx+i2]){nx2[k]=0;break;}}}cur=nx2;}
   if(cur.some(Boolean))for(let k=0;k<z.length;k++)if(mk[k]&&!cur[k])z[k]=NaN;}
  const r=fillMasked(z,mk,nx,ny);z=r.z;nHole=r.n;}   // Kontur = Rand, Lücken innen füllen
 else if($('hf').checked){const r=fillHoles(z,nx,ny,res,thr,Math.max(0,+$('hfa').value));z=r.z;nHole=r.n;}
 const tz=+$('dz').value,m=$('dzm').value;
 if(tz>0&&m==='avg')z=smooth(z,nx,ny,tz,Math.max(1,Math.round(+$('dzr').value)||1));
 if(tz>0&&m==='gauss'){const sg=Math.max(0.01,+$('gs').value),np=Math.max(1,Math.round(+$('gp').value)||1);for(let q=0;q<np;q++)z=gauss(z,nx,ny,res,sg,tz);}
 if($('fl').checked)z=flatten(z,nx,ny,res,Math.max(0.001,+$('flt').value),Math.max(0,+$('fla').value));
 const zr=Float32Array.from(z);          // geglättet, ohne Schwelle (für Umriss)
 let cnt=0;
 if(mk){for(let k=0;k<z.length;k++)if(mk[k]){if(!(z[k]>=0.01))z[k]=0.01;cnt++;}else z[k]=NaN;}   // innerhalb der Kontur alles Teil
 else for(let k=0;k<z.length;k++){if(z[k]>=thr)cnt++;else z[k]=NaN;}
 const zo=Math.max(0,+$('zoff').value||0);   // Unterlage: erst nach der Teil-Erkennung abziehen
 if(zo>0)for(let k=0;k<z.length;k++)if(z[k]===z[k])z[k]=Math.max(0.01,z[k]-zo);
 normH(z,nx,ny,res);
 const H={z,zr,P,cnt,thr,nOut,nHole};hCache={key,H};return H;}
// Höhe normieren: großräumige Wölbung abziehen (Relief bleibt), auf max. Höhe skalieren, Grunddicke darunter
function normH(z,nx,ny,res){const wr=Math.max(0,+$('nwr').value||0),mx=Math.max(0,+$('nmx').value||0),gd=Math.max(0,+$('ngd').value||0);
 if(!(wr>0||mx>0||gd>0))return;
 let r=z;
 if(wr>0){// lokale Ebene (gewichtete Regression, Kastenfilter 3× ≈ Gauß σ = Radius/2) -> kein Randfehler bei Wölbung
  const h=Math.max(1,Math.round(wr/2/res)),N=z.length,cx=nx/2,cy=ny/2,F=[];for(let q=0;q<9;q++)F.push(new Float64Array(N));
  for(let k=0;k<N;k++){const c=z[k];if(c!==c)continue;const x=(k%nx)-cx,y=Math.floor(k/nx)-cy;
   F[0][k]=1;F[1][k]=x;F[2][k]=y;F[3][k]=c;F[4][k]=x*x;F[5][k]=x*y;F[6][k]=y*y;F[7][k]=x*c;F[8][k]=y*c;}
  const S=new Float64Array(nx+1),P=new Float64Array(N+nx);
  const boxX=A=>{for(let j=0;j<ny;j++){const b=j*nx;S[0]=0;for(let t=0;t<nx;t++)S[t+1]=S[t]+A[b+t];
    for(let t=0;t<nx;t++)A[b+t]=S[Math.min(nx,t+h+1)]-S[Math.max(0,t-h)];}};
  const boxY=A=>{for(let i=0;i<nx;i++)P[i]=0;for(let k=0;k<N;k++)P[k+nx]=P[k]+A[k];   // Zeilen-Präfixsummen
    for(let j=0;j<ny;j++){const a=Math.min(ny,j+h+1)*nx,b=Math.max(0,j-h)*nx,o=j*nx;for(let i=0;i<nx;i++)A[o+i]=P[a+i]-P[b+i];}};
  for(const A of F)for(let p=0;p<3;p++){boxX(A);boxY(A);}
  r=new Float32Array(N).fill(NaN);
  for(let k=0;k<N;k++){const c=z[k];if(c!==c)continue;const x=(k%nx)-cx,y=Math.floor(k/nx)-cy;
   const w=F[0][k],mx_=F[1][k]/w,my=F[2][k]/w,mz=F[3][k]/w,sxx=F[4][k]/w-mx_*mx_,sxy=F[5][k]/w-mx_*my,syy=F[6][k]/w-my*my,
    sxz=F[7][k]/w-mx_*mz,syz=F[8][k]/w-my*mz,det=sxx*syy-sxy*sxy;
   let t=mz;if(det>1e-9*(sxx*syy+1e-12)){const gx=(sxz*syy-syz*sxy)/det,gy=(syz*sxx-sxz*sxy)/det;t=mz+gx*(x-mx_)+gy*(y-my);}
   r[k]=c-t;}}
 // Bezugspunkte robust (0,2 % / 99,8 %), damit Einzelwerte nicht skalieren
 const v=[];for(let k=0;k<r.length;k++)if(r[k]===r[k])v.push(r[k]);if(!v.length)return;v.sort((p,q)=>p-q);
 const lo=wr>0?v[Math.floor(v.length*0.002)]:0,hi=v[Math.min(v.length-1,Math.floor(v.length*0.998))];
 let sc=1;if(mx>0){const span=Math.max(1e-6,hi-lo),room=mx-gd;sc=room>0?room/span:0;}
 for(let k=0;k<z.length;k++)if(r[k]===r[k]){const q=gd+Math.max(0,r[k]-lo)*sc;z[k]=Math.max(0.01,mx>0?Math.min(mx,q):q);}}
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
// Unterseite: eben (Z=0) oder Schale (Oberseite minus Dicke, senkrecht gemessen)
function shellT(){const t=+$('sht').value;return t>0?Math.max(0.01,t):0;}
function buildMesh(kOver){const H=heights(),G=grid,k=kOver||Math.max(1,Math.round(+$('dec').value)||1);
 if(maskOn())return buildMeshCut(H,k);
 const nx=Math.floor((G.nx-1)/k)+1,ny=Math.floor((G.ny-1)/k)+1,r=G.res*k;
 const Z=(i,j)=>H.z[(j*k)*G.nx+i*k],cx=nx-1,cy=ny-1;
 if(cx<1||cy<1)return{V:new Float64Array(0),faces:[],H};
 const cell=new Uint8Array(cx*cy);
 for(let j=0;j<cy;j++)for(let i=0;i<cx;i++)cell[j*cx+i]=(isFinite(Z(i,j))&&isFinite(Z(i+1,j))&&isFinite(Z(i,j+1))&&isFinite(Z(i+1,j+1)))?1:0;
 const C=(i,j)=>i>=0&&j>=0&&i<cx&&j<cy&&cell[j*cx+i]===1;
 for(let ch=true;ch;){ch=false;for(let j=1;j<cy;j++)for(let i=1;i<cx;i++){const a=C(i-1,j-1),b=C(i,j-1),c=C(i-1,j),e=C(i,j);
  if(a&&e&&!b&&!c){cell[j*cx+i]=0;ch=true;}else if(b&&c&&!a&&!e){cell[j*cx+i-1]=0;ch=true;}}}  // nur-diagonal verbundene Zellen trennen (manifold)
 const V=[],top=new Int32Array(nx*ny).fill(-1),bot=new Int32Array(nx*ny).fill(-1);
 const vt=(i,j)=>{const q=j*nx+i;if(top[q]<0){top[q]=V.length/3;V.push(G.ox+i*r,G.oy+j*r,Z(i,j));}return top[q];};
 const sh=shellT();
 const vb=(i,j)=>{const q=j*nx+i;if(bot[q]<0){bot[q]=V.length/3;V.push(G.ox+i*r,G.oy+j*r,sh?Z(i,j)-sh:0);}return bot[q];};
 const F=[];
 for(let j=0;j<cy;j++)for(let i=0;i<cx;i++){if(!C(i,j))continue;
  const p00=vt(i,j),p10=vt(i+1,j),p11=vt(i+1,j+1),p01=vt(i,j+1),b00=vb(i,j),b10=vb(i+1,j),b11=vb(i+1,j+1),b01=vb(i,j+1);
  F.push([p00,p10,p11],[p00,p11,p01]);
  if(sh)F.push([b00,b11,b10],[b00,b01,b11]);else{const bf=[b00,b01,b11,b10];bf.bottom=true;F.push(bf);}
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
function makeStep(M,name){const V=M.V,chunks=[];let L=[],id=0;
 const E=t=>{L.push('#'+(++id)+'='+t+';');if(L.length>=20000){chunks.push(L.join('\n')+'\n');L=[];}return id;};   // stückweise -> keine Längengrenze
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
 const P=v=>{if(!cp[v]){cp[v]=E(`CARTESIAN_POINT('',(${r(V[3*v])},${r(V[3*v+1])},${r(V[3*v+2])}))`);vp[v]=E(`VERTEX_POINT('',#${cp[v]})`);}return cp[v];};
 const edges=new Map();
 const oe=(a,b)=>{const lo=Math.min(a,b),hi=Math.max(a,b),key=lo*nv+hi;let e=edges.get(key);
  if(e===undefined){P(lo);P(hi);const dx=V[3*hi]-V[3*lo],dy=V[3*hi+1]-V[3*lo+1],dz=V[3*hi+2]-V[3*lo+2];
   const vec=E(`VECTOR('',#${dir(dx,dy,dz)},${r(Math.hypot(dx,dy,dz))})`),ln=E(`LINE('',#${cp[lo]},#${vec})`);
   e=E(`EDGE_CURVE('',#${vp[lo]},#${vp[hi]},#${ln},.T.)`);edges.set(key,e);}
  return E(`ORIENTED_EDGE('',*,*,#${e},${a===lo?'.T.':'.F.'})`);};
 const loopE=vs=>E(`EDGE_LOOP('',(${vs.map((v,q)=>'#'+oe(v,vs[(q+1)%vs.length])).join(',')}))`);
 const fids=[];
 // Seiten- und Oberflächen: je Polygon eine ebene Fläche
 for(const f of M.faces){if(f.bottom)continue;
  let nx=0,ny=0,nz=0;for(let q=0;q<f.length;q++){const a=f[q]*3,b=f[(q+1)%f.length]*3;
   nx+=(V[a+1]-V[b+1])*(V[a+2]+V[b+2]);ny+=(V[a+2]-V[b+2])*(V[a]+V[b]);nz+=(V[a]-V[b])*(V[a+1]+V[b+1]);}
  const a=f[0]*3,b=f[1]*3;
  const ax=E(`AXIS2_PLACEMENT_3D('',#${P(f[0])},#${dir(nx,ny,nz)},#${dir(V[b]-V[a],V[b+1]-V[a+1],V[b+2]-V[a+2])})`);
  const pl=E(`PLANE('',#${ax})`),fb=E(`FACE_OUTER_BOUND('',#${loopE(f)},.T.)`);
  fids.push('#'+E(`ADVANCED_FACE('',(#${fb}),#${pl},.T.)`));}
 // Boden: alle Bodenflächen zu je einer ebenen Fläche pro Teil zusammenfassen (Außenrand + Löcher)
 const bot=M.faces.filter(f=>f.bottom);
 if(bot.length){const es=new Set();for(const f of bot)for(let q=0;q<f.length;q++)es.add(f[q]*nv+f[(q+1)%f.length]);
  const nxt=new Map();for(const f of bot)for(let q=0;q<f.length;q++){const a=f[q],b=f[(q+1)%f.length];if(es.has(b*nv+a))continue;
   if(!nxt.has(a))nxt.set(a,[]);nxt.get(a).push(b);}
  const loops=[];for(const[s0]of nxt){while(nxt.get(s0)&&nxt.get(s0).length){const lp=[s0];let c=nxt.get(s0).pop(),g=0;
    while(c!==s0&&g++<1e7){lp.push(c);const n=nxt.get(c);if(!n||!n.length)break;c=n.pop();}
    if(c===s0&&lp.length>=3)loops.push(lp);}}
  const xy=lp=>lp.map(v=>[V[3*v],V[3*v+1]]);
  const L2=loops.map(lp=>{const p=xy(lp);return{lp,p,a:area(p)};});
  const outs=L2.filter(o=>o.a<0),holes=L2.filter(o=>o.a>0);   // Boden von oben gesehen: außen im Uhrzeigersinn
  const bpl=E(`PLANE('',#${E(`AXIS2_PLACEMENT_3D('',#${org},#${dir(0,0,-1)},#${xAx})`)})`);
  const grp=outs.map(o=>({o,h:[]}));
  for(const h of holes){let best=null;for(const g of grp)if(inPoly(h.p[0],g.o.p)&&(!best||Math.abs(g.o.a)<Math.abs(best.o.a)))best=g;if(best)best.h.push(h);}
  for(const g of grp){const bs=[E(`FACE_OUTER_BOUND('',#${loopE(g.o.lp)},.T.)`),...g.h.map(h=>E(`FACE_BOUND('',#${loopE(h.lp)},.T.)`))];
   fids.push('#'+E(`ADVANCED_FACE('',(${bs.map(x=>'#'+x).join(',')}),#${bpl},.T.)`));}}
 const sh=E(`CLOSED_SHELL('',(${fids.join(',')}))`),msb=E(`MANIFOLD_SOLID_BREP('${name}',#${sh})`);
 const rep=E(`ADVANCED_BREP_SHAPE_REPRESENTATION('${name}',(#${wcs},#${msb}),#${ctx})`);
 const pc=E(`PRODUCT_CONTEXT('',#${app},'mechanical')`),pr=E(`PRODUCT('${name}','${name}','',(#${pc}))`);
 E(`PRODUCT_RELATED_PRODUCT_CATEGORY('part','',(#${pr}))`);
 const pdf=E(`PRODUCT_DEFINITION_FORMATION('','',#${pr})`),pdc=E(`PRODUCT_DEFINITION_CONTEXT('part definition',#${app},'design')`);
 const pd=E(`PRODUCT_DEFINITION('design','',#${pdf},#${pdc})`),pds=E(`PRODUCT_DEFINITION_SHAPE('','',#${pd})`);
 E(`SHAPE_DEFINITION_REPRESENTATION(#${pds},#${rep})`);
 chunks.push(L.join('\n')+'\n');
 const head=`ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION(('CoreXY Laser Scan'),'2;1');\nFILE_NAME('${name}.step','${new Date().toISOString().slice(0,19)}',(''),(''),'CoreXY-Scanner','CoreXY-Scanner','');\nFILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));\nENDSEC;\nDATA;\n`;
 return[head,...chunks,'ENDSEC;\nEND-ISO-10303-21;\n'];}

// ---------- Export ----------
function dl(blob,name){const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download=name;document.body.appendChild(a);a.click();
 setTimeout(()=>a.remove(),1000);
 setTimeout(()=>URL.revokeObjectURL(a.href),600000);}   // erst nach 10 min freigeben (Rückfrage „Überschreiben?“ kann dauern)
function info(H,M){let s=(H.nHole||H.nOut?`<span>Fehlstellen</span><span>${H.nHole} Loch/Löcher geschlossen, ${H.nOut} Ausreißer entfernt</span>`:'')+`<span>Z-Null-Ebene</span><span>d = ${fx(H.P[0],5)}·x + ${fx(H.P[1],5)}·y + ${fx(H.P[2])} mm</span><span>Teilpunkte</span><span>${H.cnt}</span>`;
 if(M)s+=`<span>Netz</span><span>${M.V.length/3} Vertices, ${M.faces.length} Flächen${maskOn()?', Rand = Kontur':''}${M.fails?` – <b style="color:#ff6b5b">${M.fails} Randzelle(n) fehlerhaft</b>`:''}</span>`;$('einfo').innerHTML=s;}
// ---- Scan-Datei .cxs: 64-Byte-Kopf | nx*ny int16 (Rohabstand 0.01 mm, wie gescannt) | Konturen-JSON
function cxsSave(){try{if(!grid||!grid.nx)throw new Error('Keine Scandaten');if(taskRunning())throw new Error('Scan/Kontur läuft noch');
 if(grid.rows<grid.ny)throw new Error('Scandaten noch nicht vollständig geladen');
 const{nx,ny,res,ox,oy,raw}=grid,cj=new TextEncoder().encode(JSON.stringify(cLoops)),h=new DataView(new ArrayBuffer(64));
 h.setUint32(0,0x31535843,true);h.setUint32(4,nx,true);h.setUint32(8,ny,true);h.setFloat32(12,res,true);h.setFloat32(16,ox,true);h.setFloat32(20,oy,true);
 h.setUint32(24,st.total||nx*ny,true);h.setUint32(28,st.e0||0,true);h.setUint32(32,st.e1||0,true);h.setUint32(36,st.partial?1:0,true);h.setUint32(40,cj.length,true);
 const q=new Int16Array(nx*ny);for(let k=0;k<q.length;k++){const v=raw[k];q[k]=v===v?Math.max(-32767,Math.min(32767,Math.round(v*100))):-32768;}
 const d=st.e0?new Date(st.e0*1000):new Date(),p=n=>String(n).padStart(2,'0');
 const name=($('fn').value||'scan')+'_'+d.getFullYear()+p(d.getMonth()+1)+p(d.getDate())+'-'+p(d.getHours())+p(d.getMinutes())+'.cxs';
 dl(new Blob([h.buffer,q.buffer,cj],{type:'application/octet-stream'}),name);
 $('cxst').textContent=`gespeichert: ${name} (${nx}×${ny} Punkte, ${cLoops.length} Kontur(en))`;}catch(e){$('cxst').textContent=e.message;}}
async function cxsLoad(f){if(!f)return;const T=t=>$('cxst').textContent=t;
 try{if(taskRunning())throw new Error('Scan/Kontur läuft – erst beenden');
  const b=await f.arrayBuffer(),h=new DataView(b);
  if(b.byteLength<64||h.getUint32(0,true)!==0x31535843)throw new Error('Keine Scan-Datei (.cxs)');
  const nx=h.getUint32(4,true),ny=h.getUint32(8,true),res=h.getFloat32(12,true),cl=h.getUint32(40,true);
  if(b.byteLength!==64+nx*ny*2+cl)throw new Error('Scan-Datei beschädigt (Länge passt nicht)');
  let L=[];if(cl){L=JSON.parse(new TextDecoder().decode(new Uint8Array(b,64+nx*ny*2,cl)));if(!Array.isArray(L))throw new Error('Konturen beschädigt');}
  if(!confirm(`„${f.name}“ auf den ESP übertragen?\n${nx}×${ny} Punkte, Raster ${+res.toFixed(4)} mm, ${L.length} Kontur(en).\nDer aktuelle Scan und die Konturen auf dem ESP werden ersetzt.`))return;
  const fd=new FormData();fd.append('scan',f,f.name);const x=new XMLHttpRequest(),pb=$('cxsp');pb.style.display='';
  x.upload.onprogress=e=>{if(e.lengthComputable){pb.max=e.total;pb.value=e.loaded;T('Übertragung '+Math.round(100*e.loaded/e.total)+' %  ('+(e.loaded>>10)+' / '+(e.total>>10)+' kB)');}};
  x.onload=()=>{pb.style.display='none';if(x.status!==200){T('Fehler: '+x.responseText);return;}
   cLoops=L;cSave(false);cDirty=false;grid=null;flipX=flipY=false;lastFetch=0;hCache=null;
   $('fn').value=f.name.replace(/\.cxs$/i,'').replace(/_\d{8}-\d{4}$/,'');drawC();
   T(`geladen: ${f.name} – Daten werden vom ESP gelesen …`);};
  x.onerror=()=>{pb.style.display='none';T('Übertragungsfehler');};
  x.open('POST','/api/restore');x.send(fd);T('Übertragung …');}catch(e){T(e.message);}}
async function exp(t){try{if(!grid)throw new Error('Keine Scandaten');if(st.state==='SCAN'||st.state==='PAUSED')throw new Error('Scan läuft noch');
 const fn=($('fn').value||'scan').replace(/[^\w\-]/g,'_');
 if(t==='dxf'||t==='svg'){const C=maskOn()?{loops:absLoops()}:{loops:contours().loops.map(L=>({pts:L.pts.map(([x,y])=>[x+grid.ox,y+grid.oy]),hole:L.hole}))};if(!C.loops.length)throw new Error('Kein Umriss gefunden (Mindesthöhe prüfen)');
  dl(t==='dxf'?makeDxf(C):makeSvg(C),fn+'.'+t);const nh=C.loops.filter(L=>L.hole).length;
  $('einfo').innerHTML=`<span>Umriss</span><span>${C.loops.length-nh} Außenkontur(en), ${nh} Loch/Löcher</span>`;msg('');return;}
 if(t==='csv'){const H=heights(),{nx,ny,res}=grid,L=['x_mm,y_mm,z_mm'];
  const tz=+$('dz').value,del=tz>0&&$('dzm').value==='del';
  for(let j=0;j<ny;j++){let zk=NaN;for(let i=0;i<nx;i++){const z=H.z[j*nx+i];if(!isFinite(z)){zk=NaN;continue;}
   if(del&&isFinite(zk)){const zn=i+1<nx?H.z[j*nx+i+1]:NaN;   // Lauf-Ende (Stufe/Rand) immer behalten
    if(Math.abs(z-zk)<=tz&&isFinite(zn)&&Math.abs(zn-zk)<=tz)continue;}
   zk=z;L.push(`${(grid.ox+i*res).toFixed(3)},${(grid.oy+j*res).toFixed(3)},${z.toFixed(3)}`);}}
  dl(new Blob([L.join('\n')+'\n'],{type:'text/csv'}),fn+'.csv');info(H);msg('');return;}
 const M=buildMesh();if(!M.faces.length)throw new Error('Kein zusammenhängender Teilbereich (mind. 2×2 Punkte über Schwelle nötig)');
 if(t==='3mf')dl(make3mf(M),fn+'.3mf');
 else if(t==='stl')dl(makeStl(M),fn+'.stl');
 else{   // STEP: Ausdünnung automatisch erhöhen, bis die Flächenzahl (ohne Boden) unter dem Limit liegt
  const lim=Math.max(2000,+$('smax').value||50000),nf=X=>X.faces.reduce((n,f)=>n+(f.bottom?0:1),0);   // Schale: Unterseite zählt mit (nicht als Boden markiert)
  let k=Math.max(1,Math.round(+$('dec').value)||1),M2=M,n=nf(M2),g=0;
  while(n>lim&&g++<12){k=Math.max(k+1,Math.ceil(k*Math.sqrt(n/lim)*1.05));M2=buildMesh(k);n=nf(M2);}
  msg('STEP wird erzeugt …');await new Promise(r=>setTimeout(r,30));
  const parts=makeStep(M2,fn);dl(new Blob(parts,{type:'application/step'}),fn+'.step');
  info(M2.H,M2);$('einfo').innerHTML+=`<span>STEP</span><span>Ausdünnung k = ${k}, ${n.toLocaleString('de-DE')} Flächen + Boden, ca. ${(parts.reduce((a,p)=>a+p.length,0)/1048576).toFixed(1)} MB</span>`;msg('');return;}
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
// ---------- Erklärungen beim Überfahren der Eingabefelder ----------
// ---------- Kontur abfahren ----------
let cLoops=[],cLastId=-1,cLive=null,cLastFetch=0;
try{cLoops=JSON.parse(localStorage.getItem('cLoops')||'[]');cLastId=+(localStorage.getItem('cLastId')??-1);}catch(e){}
function cSave(push=true){cVer++;try{localStorage.setItem('cLoops',JSON.stringify(cLoops));localStorage.setItem('cLastId',String(cLastId));}catch(e){}if(push)cDirty=true;}
// Konturen zusätzlich auf dem ESP speichern (überleben Browserwechsel); gesendet wird nur bei stehenden Achsen
let cDirty=false,cSyncing=false;
async function cSync(){if(!cDirty||cSyncing||taskRunning()||st.busy)return;cSyncing=true;cDirty=false;
 try{const fd=new FormData();fd.append('c',new Blob([JSON.stringify(cLoops)],{type:'application/json'}),'contour.json');
  const r=await fetch('/api/cloops',{method:'POST',body:fd});if(!r.ok)cDirty=true;}catch(e){cDirty=true;}cSyncing=false;}
// beim Start: Konturen vom ESP übernehmen; hat der ESP keine, die lokalen hochladen
(async()=>{try{const r=await fetch('/api/cloops');
 if(r.ok){const L=await r.json();if(Array.isArray(L)&&JSON.stringify(L)!==JSON.stringify(cLoops)){cLoops=L;cSave(false);drawC();}}
 else if(r.status===404&&cLoops.length)cDirty=true;}catch(e){}})();
function cStart(inner){if(inner===0&&cLoops.some(L=>!L.hole)&&!confirm('Es gibt schon eine Außenkontur. Trotzdem eine weitere abfahren?'))return;
 const q={mode:inner,dir:$('cdir').value,step:$('cstep').value,thr:$('cthr').value,ref:$('cref').value,max:$('cmax').value,f:$('cf').value,settle:$('cset').value,avg:$('cavg').value};
 cLive=[];cmd('/api/contour?'+Object.entries(q).map(([k,v])=>k+'='+encodeURIComponent(v)).join('&'));}
function cUndo(){cLoops.pop();cSave();drawC();}
function cClear(){if(cLoops.length&&!confirm('Alle Konturen löschen?'))return;cLoops=[];cSave();drawC();}
async function cFetch(final){cLastFetch=Date.now();try{const r=await fetch('/api/cdata');if(!r.ok)return;
 const id=+r.headers.get('X-Contour-Id'),closed=r.headers.get('X-Closed')==='1',mode=+r.headers.get('X-Mode'),f=new Float32Array(await r.arrayBuffer()),pts=[];
 for(let k=0;k+1<f.length;k+=2)pts.push([+f[k].toFixed(4),+f[k+1].toFixed(4)]);
 if(final){if(id!==cLastId){cLastId=id;if(closed&&pts.length>=3){const nl={pts,hole:mode===1};
   // dieselbe Kontur nochmal abgefahren (überlappt eine gleicher Art) -> ersetzen statt doppelt
   const k=cLoops.findIndex(L=>L.hole===nl.hole&&(inPoly(nl.pts[0],L.pts)||inPoly(L.pts[0],nl.pts)));
   if(k>=0){cLoops[k]=nl;msg('Überlappende '+(nl.hole?'Innen':'Außen')+'kontur ersetzt');}else cLoops.push(nl);}cSave();}cLive=null;}else cLive=pts;
 drawC();}catch(e){}}
function cPoll(){if(st.cst==null)return;
 if(st.cst===1&&Date.now()-cLastFetch>1000)cFetch(false);
 if((st.cst===2||st.cst===3)&&st.cid!==cLastId)cFetch(true);
 cInfo();if(st.cst===1)drawC();}
// importierte Konturen (imp) werden nicht nachgeglättet – so wie gezeichnet
function cProc(){const sg=Math.max(0,+$('csm').value),stp=Math.max(0.05,+$('cstep').value);
 return cLoops.map(L=>{let p=L.pts;if(sg>0&&!L.imp)p=smoothLoop(p,sg,stp);p=simplifyLoop(p,0.005);return{pts:p,hole:L.hole,area:area(p)};});}
// ---------- DXF-Import: bearbeiteten Umriss wieder als Konturen übernehmen ----------
// LINE, ARC, CIRCLE, ELLIPSE, LWPOLYLINE/POLYLINE (inkl. Bögen), SPLINE; offene Stücke werden zu geschlossenen Linienzügen verkettet.
// Außen/Loch ergibt sich aus der Verschachtelung (Loch = liegt in einer ungeraden Zahl anderer Konturen).
function dxfParse(txt){const L=txt.split(/\r?\n/),G=[];for(let i=0;i+1<L.length;i+=2)G.push([parseInt(L[i].trim(),10),L[i+1].trim()]);
 let unit=1,i=0;const ents=[];
 for(;i<G.length;i++){if(G[i][0]===9&&G[i][1]==='$INSUNITS'&&G[i+1]){const u=+G[i+1][1];unit=u===1?25.4:u===2?304.8:u===5?10:u===6?1000:1;}
  if(G[i][0]===2&&G[i][1]==='ENTITIES')break;}
 for(i++;i<G.length;){if(G[i][0]!==0){i++;continue;}const t=G[i][1];if(t==='ENDSEC'||t==='EOF')break;const e={t,c:[]};i++;
  while(i<G.length&&G[i][0]!==0){e.c.push(G[i]);i++;}ents.push(e);}
 return{ents,unit};}
const DXF_SEG=0.1;   // max. Sehnenlänge mm für Bögen/Splines
function arcPts(cx,cy,r,a0,a1){const out=[];let da=a1-a0;const n=Math.max(2,Math.ceil(Math.abs(da)*r/DXF_SEG));for(let k=0;k<=n;k++){const a=a0+da*k/n;out.push([cx+r*Math.cos(a),cy+r*Math.sin(a)]);}return out;}
function bulgePts(p,q,b){if(Math.abs(b)<1e-9)return[p,q];const th=4*Math.atan(b),dx=q[0]-p[0],dy=q[1]-p[1],ch=Math.hypot(dx,dy);if(ch<1e-12)return[p];
 const r=ch/(2*Math.sin(th/2)),h=r*Math.cos(th/2),mx=(p[0]+q[0])/2,my=(p[1]+q[1])/2,cx=mx-h*dy/ch,cy=my+h*dx/ch;
 const a0=Math.atan2(p[1]-cy,p[0]-cx);return arcPts(cx,cy,Math.abs(r),a0,a0+th);}
function nurbs(deg,K,P,W){const n=P.length;if(n<2)return P;W=W&&W.length===n?W:P.map(()=>1);
 const t0=K[deg],t1=K[n],N=Math.max(20,Math.ceil(P.reduce((a,p,k)=>k?a+Math.hypot(p[0]-P[k-1][0],p[1]-P[k-1][1]):0,0)/DXF_SEG)),out=[];
 for(let s=0;s<=N;s++){const t=Math.min(t1-1e-12,t0+(t1-t0)*s/N);let k=deg;while(k<n-1&&!(t>=K[k]&&t<K[k+1]))k++;
  const d=[];for(let j=0;j<=deg;j++){const q=P[k-deg+j],w=W[k-deg+j];d.push([q[0]*w,q[1]*w,w]);}
  for(let r=1;r<=deg;r++)for(let j=deg;j>=r;j--){const ii=k-deg+j,den=K[ii+deg-r+1]-K[ii],a=den?(t-K[ii])/den:0;for(let c=0;c<3;c++)d[j][c]=(1-a)*d[j-1][c]+a*d[j][c];}
  out.push([d[deg][0]/d[deg][2],d[deg][1]/d[deg][2]]);}
 return out;}
// nur Fit-Punkte (Spline durch Punkte): zentripetale Catmull-Rom-Kurve als gute Näherung
function catmull(F){const cl=F.length>3&&Math.hypot(F[0][0]-F[F.length-1][0],F[0][1]-F[F.length-1][1])<1e-6,Q=cl?F.slice(0,-1):F,n=Q.length,out=[];
 const at=i=>cl?Q[(i+n)%n]:Q[Math.max(0,Math.min(n-1,i))],m=cl?n:n-1;
 for(let i=0;i<m;i++){const p0=at(i-1),p1=at(i),p2=at(i+1),p3=at(i+2),dd=(a,b)=>Math.max(1e-9,Math.sqrt(Math.hypot(b[0]-a[0],b[1]-a[1])));
  const t1=dd(p0,p1),t2=t1+dd(p1,p2),t3=t2+dd(p2,p3),N=Math.max(2,Math.ceil(Math.hypot(p2[0]-p1[0],p2[1]-p1[1])/DXF_SEG));
  for(let s=0;s<N;s++){const t=t1+(t2-t1)*s/N,L=(a,b,ta,tb)=>[0,1].map(c=>((tb-t)*a[c]+(t-ta)*b[c])/(tb-ta));
   const A1=L(p0,p1,0,t1),A2=L(p1,p2,t1,t2),A3=L(p2,p3,t2,t3),B1=L(A1,A2,0,t2),B2=L(A2,A3,t1,t3);out.push(L(B1,B2,t1,t2));}}
 out.push(cl?out[0].slice():Q[n-1].slice());return out;}
function dxfImportLoops(txt){const{ents,unit}=dxfParse(txt),closed=[],open=[];const g=(e,c)=>{const v=e.c.find(x=>x[0]===c);return v?+v[1]:0;};
 const D2R=Math.PI/180;
 for(let k=0;k<ents.length;k++){const e=ents[k];
  if(e.t==='LINE')open.push([[g(e,10),g(e,20)],[g(e,11),g(e,21)]]);
  else if(e.t==='ARC'){let a0=g(e,50)*D2R,a1=g(e,51)*D2R;if(a1<=a0)a1+=2*Math.PI;open.push(arcPts(g(e,10),g(e,20),g(e,40),a0,a1));}
  else if(e.t==='CIRCLE'){const p=arcPts(g(e,10),g(e,20),g(e,40),0,2*Math.PI);p.pop();closed.push(p);}
  else if(e.t==='ELLIPSE'){const cx=g(e,10),cy=g(e,20),mx=g(e,11),my=g(e,21),ra=g(e,40)||1;let t0=g(e,41),t1=e.c.some(x=>x[0]===42)?g(e,42):2*Math.PI;if(t1<=t0)t1+=2*Math.PI;
   const R=Math.hypot(mx,my),n=Math.max(16,Math.ceil((t1-t0)*R/DXF_SEG)),p=[];for(let j=0;j<=n;j++){const t=t0+(t1-t0)*j/n,c=Math.cos(t),sn=Math.sin(t)*ra;p.push([cx+mx*c-my*sn,cy+my*c+mx*sn]);}
   if(Math.abs(t1-t0-2*Math.PI)<1e-6){p.pop();closed.push(p);}else open.push(p);}
  else if(e.t==='LWPOLYLINE'||e.t==='POLYLINE'){let V=[],cl=(g(e,70)&1)===1;
   if(e.t==='LWPOLYLINE'){for(const[c,v]of e.c){if(c===10)V.push({x:+v,y:0,b:0});else if(c===20&&V.length)V[V.length-1].y=+v;else if(c===42&&V.length)V[V.length-1].b=+v;}}
   else{while(k+1<ents.length&&ents[k+1].t==='VERTEX'){k++;const w=ents[k];if(g(w,70)&16)continue;V.push({x:g(w,10),y:g(w,20),b:g(w,42)});}if(k+1<ents.length&&ents[k+1].t==='SEQEND')k++;}
   if(V.length<2)continue;const p=[[V[0].x,V[0].y]];const nS=cl?V.length:V.length-1;
   for(let j=0;j<nS;j++){const a=V[j],b=V[(j+1)%V.length],seg=bulgePts([a.x,a.y],[b.x,b.y],a.b);for(let q=1;q<seg.length;q++)p.push(seg[q]);}
   if(cl){p.pop();closed.push(p);}else open.push(p);}
  else if(e.t==='SPLINE'){const deg=g(e,71)||3,K=[],P=[],W=[],F=[];let lx=null;
   for(const[c,v]of e.c){if(c===40)K.push(+v);else if(c===41)W.push(+v);else if(c===10)lx=[+v,0],P.push(lx);else if(c===20&&lx)lx[1]=+v;else if(c===11)F.push([+v,0]);else if(c===21&&F.length)F[F.length-1][1]=+v;}
   const p=P.length>deg&&K.length===P.length+deg+1?nurbs(deg,K,P,W):F.length>=2?catmull(F):P;if(p.length<2)continue;
   if(g(e,70)&1){const a=p[0],b=p[p.length-1];if(Math.hypot(a[0]-b[0],a[1]-b[1])<1e-6)p.pop();closed.push(p);}else open.push(p);}}
 // offene Stücke an den Enden verketten
 const tol=0.05/unit,eq=(a,b)=>Math.hypot(a[0]-b[0],a[1]-b[1])<=tol;let nOpen=0;
 const S=open.filter(p=>p.length>=2);
 while(S.length){let c=S.pop();let grown=true;
  while(grown&&!eq(c[0],c[c.length-1])){grown=false;
   for(let j=0;j<S.length;j++){const q=S[j];let add=null;
    if(eq(c[c.length-1],q[0]))add=q.slice(1);else if(eq(c[c.length-1],q[q.length-1]))add=q.slice(0,-1).reverse();
    else if(eq(c[0],q[q.length-1])){c=q.slice(0,-1).concat(c);S.splice(j,1);grown=true;break;}
    else if(eq(c[0],q[0])){c=q.slice(1).reverse().concat(c);S.splice(j,1);grown=true;break;}
    if(add){c=c.concat(add);S.splice(j,1);grown=true;break;}}}
  if(c.length>=4&&Math.hypot(c[0][0]-c[c.length-1][0],c[0][1]-c[c.length-1][1])<=Math.max(tol,0.5/unit)){c.pop();closed.push(c);}else nOpen++;}
 // Einheit: $INSUNITS; ergäbe das ein unplausibel großes Teil (> 2 m), wird mm angenommen (manche Programme setzen die Einheit falsch)
 let uu=unit;if(unit!==1){let m=0;for(const c of closed)for(const q of c)m=Math.max(m,Math.abs(q[0]),Math.abs(q[1]));if(m*unit>2000)uu=1;}
 // mm, doppelte Punkte raus, Außen/Loch über Verschachtelung
 const loops=[];for(const c of closed){const p=[];for(const q of c){const m=[+(q[0]*uu).toFixed(4),+(q[1]*uu).toFixed(4)];const l=p[p.length-1];if(!l||Math.hypot(m[0]-l[0],m[1]-l[1])>1e-4)p.push(m);}
  if(p.length>=3&&Math.abs(area(p))>1e-3)loops.push(p);}
 const out=loops.map((p,i)=>{let d=0;for(let j=0;j<loops.length;j++)if(j!==i&&Math.abs(area(loops[j]))>Math.abs(area(p))&&inPoly(p[0],loops[j]))d++;return{pts:p,hole:(d&1)===1,imp:true};});
 return{loops:out,nOpen,unit:uu,unitIgnored:uu!==unit};}
async function cImport(f){if(!f)return;try{if(taskRunning())throw new Error('Scan/Kontur läuft – erst beenden');
 const{loops,nOpen,unit,unitIgnored}=dxfImportLoops(await f.text());if(!loops.length)throw new Error('Keine geschlossenen Konturen in der DXF gefunden'+(nOpen?` (${nOpen} offene Linienzüge)`:''));
 const no=loops.filter(L=>!L.hole).length,nh=loops.length-no;
 if(cLoops.length&&!confirm(`„${f.name}“: ${no} Außen-, ${nh} Innenkontur(en).\nDie vorhandenen ${cLoops.length} Konturen werden ersetzt.`))return;
 cLoops=loops;cSave();drawC();msg(`Importiert: ${no} Außen-, ${nh} Innenkontur(en)`+(nOpen?` – ${nOpen} offene Linienzüge ignoriert (Enden nicht verbunden)`:'')+(unit!==1?` – Einheit aus DXF umgerechnet (×${unit})`:'')+(unitIgnored?' – Einheit der DXF unplausibel, als mm übernommen':''));}catch(e){msg(e.message);}}
// ---------- Maße ----------
const perim=P=>{let s=0;for(let k=0;k<P.length;k++){const a=P[k],b=P[(k+1)%P.length];s+=Math.hypot(b[0]-a[0],b[1]-a[1]);}return s;};
const bbox=P=>{let a=Infinity,b=-Infinity,c=Infinity,d=-Infinity;for(const[x,y]of P){a=Math.min(a,x);b=Math.max(b,x);c=Math.min(c,y);d=Math.max(d,y);}return[a,b,c,d];};
const nf=(v,n=1)=>isFinite(v)?v.toLocaleString('de-DE',{minimumFractionDigits:n,maximumFractionDigits:n}):'–';
function measures(){let h='';
 // aus den Konturen (genau, geglättet)
 const P=cProc(),O=P.filter(L=>!L.hole),I=P.filter(L=>L.hole);
 if(O.length){const b=bbox(O.flatMap(L=>L.pts)),A=O.reduce((a,L)=>a+Math.abs(L.area),0)-I.reduce((a,L)=>a+Math.abs(L.area),0);
  h+=`<span>Kontur L × B</span><span>${nf(b[1]-b[0],2)} × ${nf(b[3]-b[2],2)} mm (X × Y)</span>`+
     `<span>Umfang</span><span>außen ${nf(O.reduce((a,L)=>a+perim(L.pts),0))} mm${I.length?`, Ausschnitte ${nf(I.reduce((a,L)=>a+perim(L.pts),0))} mm (${I.length})`:''}</span>`+
     `<span>Fläche</span><span>${nf(A)} mm² (${nf(A/100,2)} cm²)${I.length?', Ausschnitte abgezogen':''}</span>`;}
 // aus dem Höhenscan
 const run=['SCAN','RETURN','PAUSED'].includes(st.state);
 if(grid&&grid.rows&&!run){try{const H=heights(),{nx,ny,res}=grid;let n=0,s=0,zm=0,i0=1e9,i1=-1,j0=1e9,j1=-1;
   for(let j=0;j<ny;j++)for(let i=0;i<nx;i++){const z=H.z[j*nx+i];if(!(z===z))continue;n++;s+=z;if(z>zm)zm=z;if(i<i0)i0=i;if(i>i1)i1=i;if(j<j0)j0=j;if(j>j1)j1=j;}
   if(n){const a=res*res,V=s*a;
    if(!O.length||!maskOn())h+=`<span>Scan L × B</span><span>ca. ${nf((i1-i0)*res,2)} × ${nf((j1-j0)*res,2)} mm (±${res} mm Raster)</span><span>Scan-Fläche</span><span>ca. ${nf(n*a)} mm²</span>`;
    h+=`<span>Höhe</span><span>max. ${nf(zm,2)} mm, Mittel ${nf(s/n,2)} mm</span><span>Volumen</span><span>${nf(V,0)} mm³ (${nf(V/1000,2)} cm³)</span>`;}}catch(e){}}
 else if(run)h+='<span>Scan</span><span>Maße nach Scanende</span>';
 $('meas').innerHTML=h||'<span>–</span><span>noch keine Daten</span>';}
function cInfo(){const P=cProc();let h='';
 if(st.cst===1)h+=`<span>Läuft</span><span>${st.cn} Kantenpunkte</span>`;
 if(st.cst===3&&st.cerr)h+=`<span>Fehler</span><span style="color:#ff6b5b">${st.cerr}</span>`;
 P.forEach((L,k)=>{const b=bbox(L.pts);h+=`<span>${k+1}. ${L.hole?'Innen':'Außen'}</span><span>${(b[1]-b[0]).toFixed(2)} × ${(b[3]-b[2]).toFixed(2)} mm, Umfang ${perim(L.pts).toFixed(1)} mm, ${Math.abs(L.area).toFixed(1)} mm²</span>`;});
 if(!P.length&&st.cst!==1)h+='<span>Konturen</span><span>noch keine</span>';
 $('cinfo').innerHTML=h;}
function drawC(){try{measures();}catch(e){}const cv=$('ccv'),P=cProc(),all=P.map(L=>L.pts);if(cLive&&cLive.length)all.push(cLive);
 const hx=st.x,hy=st.y;let x0=Infinity,x1=-Infinity,y0=Infinity,y1=-Infinity;if(isFinite(hx)&&isFinite(hy))all.push([[hx,hy]]);for(const L of all)for(const[x,y]of L){x0=Math.min(x0,x);x1=Math.max(x1,x);y0=Math.min(y0,y);y1=Math.max(y1,y);}
 if(!isFinite(x0)){x0=y0=0;x1=y1=10;}const m=5;x0-=m;y0-=m;x1+=m;y1+=m;
 const W=900,sc=W/Math.max(x1-x0,y1-y0,1),cw=Math.max(200,Math.round((x1-x0)*sc)),ch=Math.max(150,Math.round((y1-y0)*sc));
 cv.width=cw;cv.height=ch;const g=cv.getContext('2d');g.fillStyle='#000';g.fillRect(0,0,cw,ch);
 const T=([x,y])=>[(x-x0)*sc,ch-(y-y0)*sc];g.lineWidth=Math.max(2,cw/300);
 for(const L of P){g.strokeStyle=L.hole?'#00e5ff':'#ffffff';g.fillStyle=L.hole?'rgba(0,229,255,.08)':'rgba(255,255,255,.08)';g.beginPath();L.pts.forEach((p,k)=>{const[a,b]=T(p);k?g.lineTo(a,b):g.moveTo(a,b);});g.closePath();g.fill();g.stroke();}
 if(cLive&&cLive.length){g.strokeStyle='#ffb347';g.beginPath();cLive.forEach((p,k)=>{const[a,b]=T(p);k?g.lineTo(a,b):g.moveTo(a,b);});g.stroke();}
 if(isFinite(hx)){const[a,b]=T([hx,hy]);g.fillStyle='#ff2a2a';g.beginPath();g.arc(a,b,5,0,7);g.fill();}
 g.fillStyle='#8fa0b8';g.font=Math.max(12,Math.round(cw/40))+'px sans-serif';g.fillText(`${(x1-x0).toFixed(0)} × ${(y1-y0).toFixed(0)} mm · weiß außen · türkis innen · orange läuft · rot Kopf`,8,ch-8);}
function cExp(t){const P=cProc();if(!P.length)return msg('Keine Konturen vorhanden');
 const fn=(($('fn').value||'scan').replace(/[^\w\-]/g,'_'))+'_kontur',C={loops:P};dl(t==='dxf'?makeDxf(C):makeSvg(C),fn+'.'+t);msg('');}

// ---------- Kontur als Rand (Maske) ----------
let cVer=0,mCache=null;
function maskOn(){return !!grid&&$('cmask').checked&&cLoops.some(L=>!L.hole);}
// Konturen in Rasterkoordinaten (relativ zum Rasterursprung, Spiegelung angewendet)
function relLoops(){const W=(grid.nx-1)*grid.res,Hh=(grid.ny-1)*grid.res;
 return cProc().map(L=>({pts:L.pts.map(([x,y])=>{let u=x-grid.ox,v=y-grid.oy;if(flipX)u=W-u;if(flipY)v=Hh-v;return[u,v];}),hole:L.hole}));}
// Konturen in Maschinenkoordinaten (für Export, Spiegelung angewendet)
function absLoops(){return relLoops().map(L=>({pts:L.pts.map(([u,v])=>[u+grid.ox,v+grid.oy]),hole:L.hole}));}
// Schnittpunkte einer waagrechten Linie y mit allen Konturen (sortiert) -> innen zwischen Paaren (gerade/ungerade)
function rowX(loops,y){const X=[];for(const L of loops){const P=L.pts,n=P.length;for(let k=0;k<n;k++){const a=P[k],b=P[(k+1)%n];
 if((a[1]>y)!==(b[1]>y))X.push(a[0]+(y-a[1])/(b[1]-a[1])*(b[0]-a[0]));}}return X.sort((p,q)=>p-q);}
function maskGrid(){const key=grid.ver+'|'+cVer+'|'+$('csm').value+'|'+flipX+flipY+'|'+grid.nx+'|'+grid.ny+'|'+grid.ox+'|'+grid.oy;
 if(mCache&&mCache.key===key)return mCache.M;
 const{nx,ny,res}=grid,L=relLoops(),M=new Uint8Array(nx*ny);
 for(let j=0;j<ny;j++){const X=rowX(L,j*res);let q=0;for(let i=0;i<nx;i++){const x=i*res;while(q<X.length&&X[q]<=x)q++;if(q&1)M[j*nx+i]=1;}}
 mCache={key,M};return M;}
// Lücken innerhalb der Maske schichtweise aus den Nachbarn füllen
function fillMasked(z,M,nx,ny){const o=Float32Array.from(z);let todo=[],n=0;
 for(let k=0;k<o.length;k++)if(M[k]&&o[k]!==o[k])todo.push(k);
 while(todo.length){const up=[],rest=[];
  for(const k of todo){const i=k%nx;let s=0,c=0;
   if(i>0&&o[k-1]===o[k-1]){s+=o[k-1];c++;}if(i<nx-1&&o[k+1]===o[k+1]){s+=o[k+1];c++;}
   if(k>=nx&&o[k-nx]===o[k-nx]){s+=o[k-nx];c++;}if(k+nx<o.length&&o[k+nx]===o[k+nx]){s+=o[k+nx];c++;}
   if(c)up.push(k,s/c);else rest.push(k);}
  if(!up.length)break;for(let q=0;q<up.length;q+=2)o[up[q]]=up[q+1];n+=up.length/2;todo=rest;}
 // isolierte Punkte (z.B. einzelne Rasterpunkte in spitzen Ecken): nächsten gültigen Wert im wachsenden Umkreis nehmen
 const W=Float32Array.from(o);
 for(const k of todo){const i=k%nx,j=(k-i)/nx;let best=NaN,bd=Infinity;
  for(let R=1;R<=256&&best!==best;R*=2)for(let v=Math.max(0,j-R);v<=Math.min(ny-1,j+R);v++)for(let u=Math.max(0,i-R);u<=Math.min(nx-1,i+R);u++){
   const w=W[v*nx+u];if(w===w){const dd=(u-i)*(u-i)+(v-j)*(v-j);if(dd<bd){bd=dd;best=w;}}}
  o[k]=best===best?best:0.01;n++;}
 return{z:o,n};}
// Ohrenschneiden für einfache Polygone (gegen den Uhrzeigersinn)
function earclip(ids,X,Y){const idx=ids.slice(),T=[];const cr=(a,b,c)=>(X[b]-X[a])*(Y[c]-Y[a])-(Y[b]-Y[a])*(X[c]-X[a]);
 let guard=0;
 while(idx.length>3&&guard++<100000){let found=false;
  for(let q=0;q<idx.length;q++){const n=idx.length,a=idx[(q+n-1)%n],b=idx[q],c=idx[(q+1)%n];
   if(cr(a,b,c)<=1e-14)continue;let ok=true;
   for(const p of idx){if(p===a||p===b||p===c)continue;if(cr(a,b,p)>=-1e-14&&cr(b,c,p)>=-1e-14&&cr(c,a,p)>=-1e-14){ok=false;break;}}
   if(!ok)continue;T.push([a,b,c]);idx.splice(q,1);found=true;break;}
  if(!found)return{T,ok:false};}
 if(idx.length===3){if(cr(idx[0],idx[1],idx[2])>1e-14)T.push(idx);else return{T,ok:false};}
 return{T,ok:true};}

// 3D-Netz mit exakten Konturrändern: volle Zellen wie gewohnt, Randzellen an der Kontur zugeschnitten,
// Wände entlang der Kontur. Höhen am Rand aus den Innenpunkten fortgesetzt (bilinear).
function buildMeshCut(H,k){const G=grid,r=G.res*k,nxc=Math.ceil((G.nx-1)/k)+1,nyc=Math.ceil((G.ny-1)/k)+1,ncx=nxc-1,ncy=nyc-1;
 const F=new Float64Array(nxc*nyc).fill(NaN);
 for(let j=0;j<nyc;j++)for(let i=0;i<nxc;i++){const gi=i*k,gj=j*k;if(gi<G.nx&&gj<G.ny){const v=H.z[gj*G.nx+gi];if(v===v)F[j*nxc+i]=v;}}
 // nach außen fortsetzen, schichtweise bis alle Knoten nahe des Teils einen Wert haben (spitze Ecken: weit weg vom nächsten Innenpunkt)
 {let front=[];for(let q=0;q<F.length;q++)if(F[q]!==F[q]){const i=q%nxc,j=(q-i)/nxc;for(let v=j-1;v<=j+1;v++)for(let u=i-1;u<=i+1;u++){if(u<0||v<0||u>=nxc||v>=nyc)continue;if(F[v*nxc+u]===F[v*nxc+u]){front.push(q);v=j+2;break;}}}
  for(let it=0;it<60&&front.length;it++){const U=[],seen=new Set();
   for(const q of front){const i=q%nxc,j=(q-i)/nxc;let s=0,c=0;for(let v=j-1;v<=j+1;v++)for(let u=i-1;u<=i+1;u++){if(u<0||v<0||u>=nxc||v>=nyc)continue;const w=F[v*nxc+u];if(w===w){s+=w;c++;}}if(c)U.push(q,s/c);}
   for(let q=0;q<U.length;q+=2)F[U[q]]=U[q+1];
   const nf=[];for(let q=0;q<U.length;q+=2){const k=U[q],i=k%nxc,j=(k-i)/nxc;for(let v=j-1;v<=j+1;v++)for(let u=i-1;u<=i+1;u++){if(u<0||v<0||u>=nxc||v>=nyc)continue;const n=v*nxc+u;if(F[n]!==F[n]&&!seen.has(n)){seen.add(n);nf.push(n);}}}
   front=nf;}}
 for(let q=0;q<F.length;q++)if(!(F[q]>=0.01))F[q]=0.01;
 const hz=(x,y)=>{let i=Math.floor(x/r),j=Math.floor(y/r);i=Math.max(0,Math.min(nxc-2,i));j=Math.max(0,Math.min(nyc-2,j));const fx=x/r-i,fy=y/r-j;
  return Math.max(0.01,F[j*nxc+i]*(1-fx)*(1-fy)+F[j*nxc+i+1]*fx*(1-fy)+F[(j+1)*nxc+i]*(1-fx)*fy+F[(j+1)*nxc+i+1]*fx*fy);};
 // Konturen vorbereiten: ins Raster klemmen, nicht genau auf Rasterlinien, Orientierung außen CCW / innen CW
 const Wx=(nxc-1)*r,Wy=(nyc-1)*r;
 let loops=relLoops().map(L=>{const p=L.pts.map(([x,y])=>{x=Math.min(Wx-1e-3*r,Math.max(1e-3*r,x));y=Math.min(Wy-1e-3*r,Math.max(1e-3*r,y));
   // Mindestabstand 0,2 % der Rasterweite zu Rasterlinien -> keine Splitter-Dreiecke
   const sn=v=>{const f=v/r-Math.round(v/r);return Math.abs(f)<2e-3?(Math.round(v/r)+(f<0?-2e-3:2e-3))*r:v;};return[sn(x),sn(y)];});
  const q=[];for(const P of p){const l=q[q.length-1];if(!l||Math.hypot(P[0]-l[0],P[1]-l[1])>5e-3*r)q.push(P);}   // fast doppelte Punkte weg
  while(q.length>3&&Math.hypot(q[0][0]-q[q.length-1][0],q[0][1]-q[q.length-1][1])<=5e-3*r)q.pop();
  const A=area(q);if(L.hole?A>0:A<0)q.reverse();return{pts:q,hole:L.hole};});
 loops=loops.filter(L=>{let a=Infinity,b=-Infinity,c=Infinity,d=-Infinity;for(const[x,y]of L.pts){a=Math.min(a,x);b=Math.max(b,x);c=Math.min(c,y);d=Math.max(d,y);}
  return Math.floor(a/r)!==Math.floor(b/r)||Math.floor(c/r)!==Math.floor(d/r);});
 // 2D-Punkte: Rasterknoten 0..NN-1, danach Kontur- und Schnittpunkte
 const NN=nxc*nyc,PX=[],PY=[],isB=[];
 const px=id=>id<NN?(id%nxc)*r:PX[id-NN],py=id=>id<NN?Math.floor(id/nxc)*r:PY[id-NN];
 const addP=(x,y,b)=>{PX.push(x);PY.push(y);isB.push(b);return NN+PX.length-1;};
 const segs=new Map(),allSeg=[];
 for(const L of loops){const P=L.pts,n=P.length,vid=P.map(([x,y])=>addP(x,y,false));
  for(let q=0;q<n;q++){const[ax,ay]=P[q],[bx,by]=P[(q+1)%n],S=[];
   for(let i=Math.ceil(Math.min(ax,bx)/r);i<=Math.floor(Math.max(ax,bx)/r);i++){const t=(i*r-ax)/(bx-ax);if(t>0&&t<1)S.push([t,i*r,ay+t*(by-ay),0]);}
   for(let j=Math.ceil(Math.min(ay,by)/r);j<=Math.floor(Math.max(ay,by)/r);j++){const t=(j*r-ay)/(by-ay);if(t>0&&t<1)S.push([t,ax+t*(bx-ax),j*r,1]);}
   S.sort((u,w)=>u[0]-w[0]);
   const ch=[vid[q],...S.map(([,x,y])=>addP(x,y,true)),vid[(q+1)%n]];
   for(let m=0;m+1<ch.length;m++){const u=ch[m],w=ch[m+1],mx=(px(u)+px(w))/2,my=(py(u)+py(w))/2;
    const ci=Math.min(ncx-1,Math.max(0,Math.floor(mx/r))),cj=Math.min(ncy-1,Math.max(0,Math.floor(my/r))),key=cj*ncx+ci;
    if(!segs.has(key))segs.set(key,[]);segs.get(key).push([u,w]);allSeg.push([u,w]);}}}
 // 3D-Eckpunkte (oben/unten) bei Bedarf
 const V=[],vT=new Map(),vB=new Map();
 const vt=id=>{let v=vT.get(id);if(v===undefined){v=V.length/3;const x=px(id),y=py(id);V.push(x,y,id<NN?F[id]:hz(x,y));vT.set(id,v);}return v;};
 const sh=shellT();
 const vb=id=>{let v=vB.get(id);if(v===undefined){v=V.length/3;const x=px(id),y=py(id);V.push(x,y,sh?(id<NN?F[id]:hz(x,y))-sh:0);vB.set(id,v);}return v;};
 const Fc=[];let fails=0;
 const eps=1e-7*r;
 for(let cj=0;cj<ncy;cj++){const Xr=rowX(loops,(cj+0.5)*r);let qx=0;
  for(let ci=0;ci<ncx;ci++){const n00=cj*nxc+ci,n10=n00+1,n01=n00+nxc,n11=n01+1,S=segs.get(cj*ncx+ci);
   if(!S){const xc=(ci+0.5)*r;while(qx<Xr.length&&Xr[qx]<=xc)qx++;if(!(qx&1))continue;
    Fc.push([vt(n00),vt(n10),vt(n11)],[vt(n00),vt(n11),vt(n01)]);if(sh)Fc.push([vb(n00),vb(n11),vb(n10)],[vb(n00),vb(n01),vb(n11)]);else{const bq=[vb(n00),vb(n01),vb(n11),vb(n10)];bq.bottom=true;Fc.push(bq);}continue;}
   const x0=ci*r,y0=cj*r,x1=x0+r,y1=y0+r;
   const tp=id=>{const x=px(id),y=py(id);if(Math.abs(y-y0)<eps)return(x-x0)/r;if(Math.abs(x-x1)<eps)return 1+(y-y0)/r;if(Math.abs(y-y1)<eps)return 2+(x1-x)/r;if(Math.abs(x-x0)<eps)return 3+(y1-y)/r;return -1;};
   const start=new Map();for(const s of S)start.set(s[0],s);
   const chains=[];
   for(const s of S){if(!isB[s[0]-NN])continue;const ids=[s[0]];let c=s,g=0;
    while(c&&g++<100000){ids.push(c[1]);if(isB[c[1]-NN])break;c=start.get(c[1]);}
    chains.push({ids,ts:tp(ids[0]),te:tp(ids[ids.length-1]),used:false});}
   const corner=[n00,n10,n11,n01];   // Parameter 0(=4),1,2,3
   for(const c0 of chains){if(c0.used)continue;const poly=[];let cur=c0;c0.used=true;let g=0;
    for(;;){if(g++>1000){fails++;break;}poly.push(...cur.ids);const t=cur.te;let best=null,bd=9;
     for(const c of chains){if(c.used&&c!==c0)continue;const dd=((c.ts-t)%4+4)%4;if(dd>1e-12&&dd<bd){bd=dd;best=c;}}
     if(!best){fails++;break;}
     const cs=[];for(let q=0;q<4;q++){const dd=((q-t)%4+4)%4;if(dd>1e-12&&dd<bd)cs.push([dd,corner[q]]);}cs.sort((u,w)=>u[0]-w[0]);for(const[,id]of cs)poly.push(id);
     if(best===c0)break;best.used=true;cur=best;}
    const pxs=new Map(),X={},Y={};for(const id of poly){X[id]=px(id);Y[id]=py(id);}
    const E=earclip(poly,X,Y);if(!E.ok)fails++;
    for(const[a,b,c]of E.T){Fc.push([vt(a),vt(b),vt(c)]);const bt=[vb(a),vb(c),vb(b)];bt.bottom=!sh;Fc.push(bt);}}}}
 for(const[u,w]of allSeg)Fc.push([vb(u),vb(w),vt(w),vt(u)]);   // Wände entlang der Kontur
 for(let q=0;q<V.length;q+=3){V[q]+=G.ox;V[q+1]+=G.oy;}
 return{V:Float64Array.from(V),faces:Fc,H,fails};}

async function cScanInside(){try{if(taskRunning())throw new Error('Es läuft bereits ein Vorgang');
 const L=cProc();if(!L.some(l=>!l.hole))throw new Error('Erst eine Außenkontur abfahren');
 const res=+$('res').value;if(!(res>0))throw new Error('Auflösung in den Scan-Einstellungen prüfen');
 const m=2*res;let x0=Infinity,x1=-Infinity,y0=Infinity,y1=-Infinity;
 for(const l of L)if(!l.hole)for(const[x,y]of l.pts){x0=Math.min(x0,x);x1=Math.max(x1,x);y0=Math.min(y0,y);y1=Math.max(y1,y);}
 const ox=x0-m,oy=y0-m,nx=Math.floor((x1+m-ox)/res+1e-6)+1,ny=Math.floor((y1+m-oy)/res+1e-6)+1,rows=[];let tot=0;
 for(let j=0;j<ny;j++){const raw=[];   // Abschnitte bei y und y±Rand (Rand auch in Y-Richtung)
  for(const dy of[-m,-m/2,0,m/2,m]){const X=rowX(L,oy+j*res+dy);
   for(let q=0;q+1<X.length;q+=2){const a=Math.max(0,Math.ceil((X[q]-m-ox)/res-1e-9)),b=Math.min(nx-1,Math.floor((X[q+1]+m-ox)/res+1e-9));if(a<=b)raw.push([a,b]);}}
  raw.sort((u,w)=>u[0]-w[0]);const iv=[];
  for(const[a,b]of raw){if(iv.length&&a<=iv[iv.length-1][1]+1)iv[iv.length-1][1]=Math.max(iv[iv.length-1][1],b);else iv.push([a,b]);}
  for(const[a,b]of iv)tot+=b-a+1;rows.push([iv.length,...iv.flat()].join(' '));}
 if(!tot)throw new Error('Keine Fläche innerhalb der Kontur');
 if(!confirm(`Fläche innerhalb der Kontur scannen:\n${tot.toLocaleString('de-DE')} Punkte statt ${(nx*ny).toLocaleString('de-DE')} im umgebenden Rechteck.\nRaster ${res} mm, Bereich ${(x1-x0).toFixed(1)} × ${(y1-y0).toFixed(1)} mm. Starten?`))return;
 const q=['res','feed','travel','settle','avg','lat'].map(k=>k+'='+encodeURIComponent($(k).value)).join('&')+
  `&bidir=${$('bidir').checked?1:0}&fly=${$('fly').checked?1:0}&ox=${ox.toFixed(4)}&oy=${oy.toFixed(4)}&nx=${nx}&ny=${ny}`;
 const r=await fetch('/api/scanmask?'+q,{method:'POST',headers:{'Content-Type':'text/plain'},body:rows.join('\n')});
 const t=await r.text();if(!r.ok)throw new Error(t);
 grid=null;flipX=flipY=false;$('cmask').checked=true;msg('');}catch(e){msg(e.message);}}
function taskRunning(){return['SCAN','RETURN','PAUSED','FRAME','CONTOUR'].includes(st.state);}

// ---------- Jog: kurz tippen = Schritt, gedrückt halten = Dauerfahrt ----------
document.querySelectorAll('.jb').forEach(b=>{const dx=+b.dataset.dx,dy=+b.dataset.dy;let t=null,cont=false,ka=null;
 const post=u=>fetch(u,{method:'POST'}).catch(()=>{});
 const down=e=>{e.preventDefault();try{b.setPointerCapture(e.pointerId);}catch(x){}cont=false;
  t=setTimeout(()=>{t=null;cont=true;post(`/api/jogc?dx=${dx}&dy=${dy}&f=${$('jf').value}`);ka=setInterval(()=>post('/api/jogka'),200);},250);};
 const up=()=>{if(t){clearTimeout(t);t=null;jog(dx,dy);}else if(cont){cont=false;clearInterval(ka);post('/api/jogstop');}};
 b.addEventListener('pointerdown',down);b.addEventListener('pointerup',up);b.addEventListener('pointercancel',up);b.addEventListener('lostpointercapture',up);
 b.addEventListener('contextmenu',e=>e.preventDefault());});

const TIPS={
 w:'Größe des Scanbereichs in X ab der aktuellen Position (mm). Der Startpunkt ist die linke vordere Ecke.',
 h:'Größe des Scanbereichs in Y ab der aktuellen Position (mm).',
 res:'Abstand der Messpunkte in X und Y (mm). Kleiner = feiner, aber deutlich mehr Punkte und längere Scanzeit. Sinnvoll: 0,25–1 mm.',
 feed:'Geschwindigkeit beim Scannen (mm/s). Bei Anhalten an jedem Punkt bremst bei kleinem Raster meist die Beschleunigung, bei fliegender Messung ist das die Zeilengeschwindigkeit.',
 travel:'Geschwindigkeit für Positionierfahrten (Zeilenanfang, Rückfahrt, Rahmen) in mm/s.',
 settle:'Wartezeit nach dem Anfahren eines Punkts, damit Mechanik und Sensor ruhig sind (ms). Nur bei Anhalten an jedem Punkt. Empfohlen 60–100 ms.',
 avg:'Anzahl gemittelter Messwerte pro Punkt. Mehr = weniger Rauschen, aber langsamer. ESP32-ADC: 32, ADS1115: 8–16 (je ca. 1,2 ms).',
 bidir:'An: Zeilen abwechselnd hin und zurück (schneller). Aus: jede Zeile aus derselben Richtung (genauer, kein Zeilenversatz).',
 fly:'Zeile ohne Anhalten durchfahren und beim Überfahren jedes Rasterpunkts messen. Deutlich schneller. Vor und hinter der Zeile wird etwas über den Bereich hinausgefahren.',
 lat:'Nur fliegend: gleicht die Laufzeit von Sensor und Wandler aus. Einstellen an einer scharfen Kante mit Serpentine, bis die Kante gerade ist. Standard 2 ms.',
 bm:'Wie die Auflagefläche (Z = 0) bestimmt wird. Auto: Ebene durch die am weitesten entfernten Punkte, gleicht Schräglage aus. Startpunkt: Messwert am ersten Punkt. Manuell: fester Abstand.',
 dman:'Nur bei Z-Null „Manuell“: Abstand Sensor–Auflage in mm.',
 tol:'Auto-Ebene: maximale Abweichung, bis zu der ein Punkt zur Auflage zählt (mm). Etwa 3 × Rauschen.',
 thr:'Punkte niedriger als dieser Wert gelten als Auflage und werden nicht exportiert (mm). Auf dieser Höhe liegt auch der Umriss. Etwa 5 × Rauschen.',
 hf:'Schließt kleine Löcher im Teil (Messausfälle, z. B. an glänzenden Stellen) durch glatte Interpolation aus der Umgebung.',
 hfa:'Löcher bis zu dieser Fläche werden geschlossen (mm²). Größere gelten als echte Bohrungen und bleiben offen. Knapp unter das kleinste echte Loch stellen.',
 ol:'Einzelne Spitzen, die von allen Nachbarn um mehr als diesen Wert abweichen, werden verworfen und gefüllt (mm). Kanten bleiben erhalten. 0 = aus, sinnvoll 0,5–1 mm.',
 dzm:'Gauß: gewichtetes Glätten, Kanten bleiben scharf (empfohlen). Mittelwert: einfacher Mittelwert. Löschen: entfernt in der CSV Punkte gleicher Höhe.',
 dz:'Schaltet das Glätten ein (0 = aus). Nachbarn mit mehr als ca. 3 × diesem Höhenunterschied gelten als Kante und werden nicht einbezogen. Etwa 3 × Rauschen.',
 gs:'Reichweite der Gauß-Glättung (mm). Größer = glatter, feine Details gehen verloren. Sinnvoll 0,3–1 mm.',
 gp:'Wie oft die Gauß-Glättung angewendet wird. Mehr Durchläufe = stärker.',
 dzr:'Nur Modus Mittelwert: Nachbarschaft in Punkten (1 = 3×3, 2 = 5×5).',
 fl:'Erkennt ebene Bereiche und setzt sie exakt auf eine Ebene. Gekrümmte Flächen bleiben unverändert.',
 flt:'Maximale Abweichung eines Punkts von der erkannten Ebene (mm). Sinnvoll 0,03–0,1 mm.',
 fla:'Kleinere ebene Bereiche werden nicht begradigt (mm²).',
 cs:'Glättung der Umrisslinie (mm). Größer = runder, Ecken werden weicher und Löcher etwas kleiner. 0 = ungeglättet.',
 cma:'Konturen kleiner als diese Fläche gelten als Störung und werden weggelassen (mm²).',
 cpv:'Zeigt den Umriss in der Vorschau: Außenkonturen weiß, Löcher türkis.',
 sht:'0 = das Modell reicht von der Oberfläche bis zur Auflage (Z = 0). Größer 0 = Schale: die Unterseite folgt der Oberfläche in diesem Abstand (senkrecht in Z gemessen) – ein gleichmäßig dickes Formteil für 3MF, STL und STEP. An steilen Flanken ist die Wand quer zur Fläche etwas dünner.',
 smax:'STEP wird bei vielen Flächen sehr groß und von CAD-Programmen kaum noch verarbeitet. Die Ausdünnung für STEP wird automatisch so erhöht, dass höchstens so viele Flächen entstehen (ca. 1 MB je 1000 Flächen). 3MF und STL bleiben unverändert.',
 dec:'Nur jeder k-te Punkt wird für 3MF, STL und STEP verwendet. Verkleinert große Dateien. Für STEP bei großen Scans 2–4.',
 fn:'Dateiname der Exporte ohne Endung.',
 js:'Weg pro Klick auf eine Pfeiltaste (mm).',
 jf:'Geschwindigkeit für manuelles Verfahren und „Zum Nullpunkt“ (mm/s).',
 calH:'Tatsächliche Höhe des Endmaßes (mm). Ideal 20–50 mm.',
 calN:'Anzahl gemittelter Messwerte pro Kalibrierpunkt. Mehr = genauer.',
 otaf:'Firmware-Datei wählen: „CoreXY_Scanner.ino.bin“ aus dem build-Ordner (Sketch → Kompilierte Binärdatei exportieren).',
 ebw:'Nur mit „Kontur als Rand“: In diesem Streifen innerhalb der Kontur werden Kantenartefakte automatisch entfernt – Punkte unter der Kantenhöhe und Punkte, die mehr als 1 mm tiefer liegen als die Punkte weiter innen. Echte Fasen bleiben erhalten. Größer = gründlicher, aber auch echte Stufen nahe am Rand würden geglättet. 0 = aus.',
 eb:'Nur mit „Kontur als Rand“: Messwerte in diesem Streifen entlang der Kontur werden verworfen und die Oberfläche von innen bis zur Kante fortgesetzt. Entfernt Grate oder Abfälle am Rand durch halb getroffene Kanten. 1–2 Rasterweiten, z. B. 0,3–0,5 mm.',
 nwr:'Zieht die großräumige Form ab (z. B. ein gewölbtes oder welliges Blatt) und behält nur das feine Relief (Adern, Struktur). Radius etwa so groß wie die Wellen, typisch 10–20 mm. 0 = aus.',
 nmx:'Skaliert die Höhen so, dass das Teil inkl. Grunddicke genau diese Höhe hat. Ohne „Wölbung entfernen“ wird nur gleichmäßig gestaucht. 0 = aus.',
 ngd:'Wird unter das (normierte) Relief gesetzt, damit dünne Teile druckbar bleiben. Zählt bei „Max. Höhe“ mit.',
 zoff:'Dicke einer Unterlage, auf die das Teil gelegt wurde (z. B. damit flache Teile eine erkennbare Kante haben). Wird nach der Teil-Erkennung von allen Höhen abgezogen – Export, Maße und Live-Z beziehen sich dann auf die Unterseite des Teils. Die Mindesthöhe gilt weiter ab Auflage.',
 cmask:'Nutzt die abgefahrenen Konturen als exakten Rand: alles innerhalb der Außenkontur und außerhalb der Ausschnitte ist Teil (Mindesthöhe wird nicht verwendet). 3MF/STL/STEP bekommen senkrechte, scharfe Kanten entlang der Kontur. Wird bei „Fläche innerhalb scannen“ automatisch gesetzt.',
 cdir:'In welche Richtung vom Startpunkt aus das Teil gesucht wird. Den Kopf vorher neben das Teil auf die Auflage fahren (bei Innenkontur: in den Ausschnitt).',
 cstep:'Abstand der Messpunkte entlang der Kante (mm). Kleiner = feinere Details, aber länger. Einschnitte schmaler als die Schrittweite können übersprungen werden. Sinnvoll 0,3–1 mm.',
 cthr:'Ab dieser Höhe über der Auflage (am Startpunkt gemessen) gilt ein Punkt als Material. Die Kontur liegt auf dieser Höhe.',
 cref:'Wie oft der Abschnitt zwischen „frei“ und „Material“ halbiert wird, um die Kante genau zu treffen. 2 = Schrittweite/8 genau. Jede Halbierung kostet eine Messung pro Kantenpunkt.',
 cmax:'Sicherheitsgrenze: so weit darf sich der Kopf vom Startpunkt entfernen (in X und in Y, mm). Wird sie erreicht, bricht die Fahrt ab.',
 cf:'Verfahrgeschwindigkeit beim Abtasten (mm/s). Bei kleinen Schritten begrenzt meist die Beschleunigung.',
 cset:'Wartezeit vor jeder Messung (ms).',
 cavg:'Gemittelte Messwerte pro Punkt. ADS1115: 4–8, ESP32-ADC: 16–32.',
 csm:'Glättung der abgefahrenen Kontur (mm). 0 = Rohdaten. Wirkt sofort auf Anzeige und Export.',
 pv:'Zeigt statt der Rohabstände die Höhe des Teils mit allen Export-Einstellungen (Z-Null, Mindesthöhe, Fehlstellen, Glättung).'};
const tipEl=document.createElement('div');tipEl.id='tip';document.body.appendChild(tipEl);
for(const[id,t]of Object.entries(TIPS)){const el=$(id);if(el)(el.closest('label')||el).dataset.tip=t;}
let tipFor=null;
function showTip(l){if(tipFor===l)return;tipFor=l;tipEl.textContent=l.dataset.tip;tipEl.style.display='block';
 const r=l.getBoundingClientRect(),tw=tipEl.offsetWidth,th=tipEl.offsetHeight;
 let x=Math.min(Math.max(8,r.left),innerWidth-tw-8),y=r.bottom+6;if(y+th>innerHeight-8)y=r.top-th-6;
 tipEl.style.left=x+'px';tipEl.style.top=y+'px';}
function hideTip(){tipFor=null;tipEl.style.display='none';}
document.addEventListener('mouseover',e=>{const l=e.target.closest&&e.target.closest('[data-tip]');l?showTip(l):hideTip();});
document.addEventListener('focusin',e=>{const l=e.target.closest&&e.target.closest('[data-tip]');l?showTip(l):hideTip();});   // Touch: beim Antippen
document.addEventListener('focusout',hideTip);
addEventListener('scroll',hideTip,true);

updZ();
drawC();cInfo();measures();
poll();
</script></body></html>
)rawliteral";
