#pragma once
// The control page served at "/". Plain HTML/CSS/JS plus one font (mona_sans.h), all served by the
// device, so it works offline on the setup network. The page is quiet matte hardware; only the glass
// glows in the phosphor color: the screen, and the mood previews, drawn in the device's own 5x7 font.

static const char WEB_PAGE[] = R"HTML(<!doctype html><html lang=en><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>CRTFace</title>
<meta name=theme-color content="#e4e6e9" media="(prefers-color-scheme: light)">
<meta name=theme-color content="#191b1e" media="(prefers-color-scheme: dark)">
<link rel=preload href=/mona-sans.woff2 as=font type=font/woff2 crossorigin><style>
@font-face{font-family:"Mona Sans";src:url(/mona-sans.woff2) format("woff2");font-weight:200 900;
 font-stretch:75% 125%;font-display:swap}
:root{--bg:#e4e6e9;--key:#f4f5f6;--key-hi:#fff;--ink:#16181b;--muted:#5f656e;--line:#c8ccd2;
 --glass:#060807;--ph:#3cff6e;--sans:"Mona Sans",system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
 --mono:ui-monospace,"SF Mono",Menlo,Consolas,monospace}
@media (prefers-color-scheme:dark){:root{--bg:#191b1e;--key:#222529;--key-hi:#2a2e33;--ink:#e9ebee;
 --muted:#959ba4;--line:#373b41}}
*{box-sizing:border-box}
html{background:var(--bg)}
body{margin:0;color:var(--ink);font:500 15px/1.45 var(--sans);-webkit-tap-highlight-color:transparent}
main{max-width:520px;margin:0 auto;padding:22px 16px 48px}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:18px}
h1{margin:0;font:760 24px/1 var(--sans);font-stretch:125%;letter-spacing:-.015em}
#st{display:flex;align-items:center;gap:8px;font-size:13px;color:var(--muted)}
.dot{width:7px;height:7px;border-radius:50%;background:var(--line)}
.lit .dot{background:var(--ph);box-shadow:0 0 6px var(--ph)}
/* the screen: dark glass in a thin bezel */
.bezel{padding:10px;border-radius:30px;background:#0b0d0f;box-shadow:inset 0 0 0 1px rgba(255,255,255,.07)}
#pad{position:relative;aspect-ratio:1;border-radius:22px/25px;overflow:hidden;touch-action:none;cursor:crosshair;
 background:radial-gradient(ellipse at 50% 42%,#121814,var(--glass) 72%);box-shadow:inset 0 0 40px #000}
#pad::after{content:"";position:absolute;inset:0;pointer-events:none;border-radius:inherit;
 background:repeating-linear-gradient(to bottom,transparent 0 2px,rgba(0,0,0,.4) 2px 3px),
 radial-gradient(ellipse at 28% 18%,rgba(255,255,255,.06),transparent 45%)}
#pad.lock{cursor:none}
#cv{position:absolute;inset:0;width:100%;height:100%}
#padhint{position:absolute;left:0;right:0;bottom:7%;margin:0;text-align:center;font:13px/1.4 var(--mono);
 color:var(--ph);opacity:.75;pointer-events:none;padding:0 16px}
/* controls */
button{font:inherit;color:inherit;margin:0;cursor:pointer}
button:focus-visible{outline:2px solid var(--ink);outline-offset:2px}
.btn,.opt{border:1px solid var(--line);border-radius:12px;background:var(--key);
 transition:background-color 120ms,border-color 120ms,color 120ms,transform 80ms}
.btn:hover,.opt:hover{background:var(--key-hi)}
.btn:active,.opt:active{transform:scale(.97)}
.btn{display:flex;align-items:center;justify-content:space-between;gap:10px;min-height:46px;padding:0 14px;
 font-weight:580;text-align:left}
#cap{justify-content:center}
.row{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:12px}
#anywhere{width:100%;margin-top:8px}
/* on/off switch inside toggle buttons */
.sw{position:relative;flex:none;width:30px;height:18px;border-radius:9px;background:var(--line);transition:background-color 150ms}
.sw::after{content:"";position:absolute;top:2px;left:2px;width:14px;height:14px;border-radius:50%;
 background:#f6f7f8;box-shadow:0 1px 2px rgba(0,0,0,.3);transition:transform 150ms,background-color 150ms}
[aria-pressed=true] .sw{background:var(--ink)}
[aria-pressed=true] .sw::after{transform:translateX(12px);background:var(--bg)}
.note{margin:10px 2px 0;max-width:60ch;color:var(--muted);font:400 13.5px/1.5 var(--sans)}
section{margin-top:30px}
h2{margin:0 2px 10px;font:680 15px/1.2 var(--sans);font-stretch:112%}
/* screen picker: one segmented control */
.seg{display:grid;grid-template-columns:repeat(4,1fr);gap:3px;padding:3px;border:1px solid var(--line);
 border-radius:13px;background:var(--key)}
.seg button{min-width:0;min-height:40px;border:0;border-radius:9px;background:none;color:var(--muted);
 font-size:14px;font-weight:600;transition:background-color 120ms,color 120ms}
.seg button:hover{color:var(--ink)}
.seg [aria-pressed=true],.seg [aria-pressed=true]:hover{background:var(--ink);color:var(--bg)}
/* moods and phosphors */
.grid{display:grid;gap:8px;grid-template-columns:repeat(3,1fr)}
.grid.four{grid-template-columns:repeat(4,1fr)}
.opt{display:flex;flex-direction:column;align-items:center;gap:8px;min-width:0;padding:6px 6px 9px;
 color:var(--muted);font-size:13px;font-weight:600}
.opt[aria-pressed=true]{border-color:var(--ink);color:var(--ink);box-shadow:inset 0 0 0 1px var(--ink)}
.win{display:grid;place-items:center;width:100%;height:46px;border-radius:7px;background:var(--glass)}
.win canvas{width:68px;height:28px;opacity:.3;transition:opacity 150ms}
[aria-pressed=true] .win canvas{opacity:1}
.four .opt{flex-direction:row;justify-content:center;min-height:44px;padding:0 6px}
.four i{flex:none;width:14px;height:14px;border-radius:50%;background:radial-gradient(circle,currentColor 0 3px,var(--glass) 4px)}
.four [aria-pressed=true] i{box-shadow:0 0 6px currentColor}
.four .opt i{color:var(--c)}
/* wide screens: the screen on the left, controls on the right */
@media (min-width:900px){main{max-width:1000px;display:grid;grid-template-columns:minmax(0,1fr) 380px;
 column-gap:40px;align-items:start}header{grid-column:1/-1}.stage{position:sticky;top:22px}
 .panel section:first-child{margin-top:0}}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
</style></head><body><main>
<header><h1>CRTFace</h1><span id=st role=status><span class=dot></span><span id=sttext>Connecting…</span></span></header>

<div class=stage>
<div class=bezel><div id=pad><canvas id=cv></canvas><p id=padhint></p></div></div>
<div class=row>
 <button class=btn id=cap>Capture mouse</button>
 <button class=btn id=hold aria-pressed=false>Hold click<span class=sw></span></button>
</div>
<button class=btn id=anywhere aria-pressed=false>Follow my cursor across the page<span class=sw></span></button>
<p class=note>Capture mouse sends your mouse straight to the device (Esc lets go).
On a phone, drag to move, tap to click, and turn on Hold click to drag windows.</p>
</div>

<div class=panel>
<section><h2>Screen</h2><div class=seg id=modes>
 <button data-m=0 aria-pressed=false>Face</button>
 <button data-m=1 aria-pressed=false>Music</button>
 <button data-m=2 aria-pressed=false>Weather</button>
 <button data-m=3 aria-pressed=false>Desktop</button></div>
<p class=note>The BOOT button on the board steps through Face, Music and Weather.</p></section>

<section><h2>Mood</h2><div class=grid id=exprs>
 <button class=opt data-e=0 aria-pressed=false><span class=win aria-hidden=true><canvas data-f="o_o"></canvas></span>Neutral</button>
 <button class=opt data-e=1 aria-pressed=false><span class=win aria-hidden=true><canvas data-f="^w^"></canvas></span>Happy</button>
 <button class=opt data-e=2 aria-pressed=false><span class=win aria-hidden=true><canvas data-f=";_;"></canvas></span>Sad</button>
 <button class=opt data-e=3 aria-pressed=false><span class=win aria-hidden=true><canvas data-f="&gt;_&lt;"></canvas></span>Angry</button>
 <button class=opt data-e=4 aria-pressed=false><span class=win aria-hidden=true><canvas data-f="OoO"></canvas></span>Surprised</button>
 <button class=opt data-e=5 aria-pressed=false><span class=win aria-hidden=true><canvas data-f="-_-"></canvas></span>Sleepy</button>
</div></section>

<section><h2>Phosphor</h2><div class="grid four" id=themes>
 <button class=opt data-t=0 aria-pressed=false style=--c:#3cff6e><i></i>Green</button>
 <button class=opt data-t=1 aria-pressed=false style=--c:#ffb000><i></i>Amber</button>
 <button class=opt data-t=2 aria-pressed=false style=--c:#aadcff><i></i>Ice</button>
 <button class=opt data-t=3 aria-pressed=false style=--c:#ff6ec8><i></i>Pink</button>
</div></section>
</div>
</main><script>
const W=480,$=id=>document.getElementById(id),pad=$('pad'),cv=$('cv'),g=cv.getContext('2d');
const PH=['#3cff6e','#ffb000','#aadcff','#ff6ec8'],HINT=['Point here and the face follows. Click to boop.','','','Point here to move the cursor. Click to click.'];
const still=matchMedia('(prefers-reduced-motion: reduce)');
let ws,x=240,y=240,b=0,dirty=false,hold=false,anywhere=false,mode=0,ph=PH[0],touched=false;
const clamp=v=>Math.max(0,Math.min(W-1,v));
// Mood previews in the device's own 5x7 font (glcdfont, one byte per column, top row = bit 0),
// three letters 6 dots apart like face.cpp draws them.
const GL={o:[56,68,68,68,56],_:[64,64,64,64,64],'^':[4,2,1,2,4],w:[60,64,48,64,60],';':[0,64,52,0,0],
 '>':[0,65,34,20,8],'<':[0,8,20,34,65],O:[62,65,65,65,62],'-':[8,8,8,8,8]};
function faces(){const d=devicePixelRatio||1,s=4*d;
 for(const c of document.querySelectorAll('.win canvas')){c.width=17*s;c.height=7*s;
  const k=c.getContext('2d');k.fillStyle=k.shadowColor=ph;k.shadowBlur=3*d;
  [...c.dataset.f].forEach((ch,i)=>GL[ch].forEach((bits,col)=>{
   for(let r=0;r<7;r++)if(bits>>r&1)k.fillRect((i*6+col)*s+d/2,r*s+d/2,s-d,s-d)}))}}
function status(on){$('st').classList.toggle('lit',on);$('sttext').textContent=on?'Connected':'Reconnecting…'}
function connect(){
 ws=new WebSocket(`ws://${location.host}/ws`);
 ws.onopen=()=>status(true);
 ws.onclose=()=>{status(false);setTimeout(connect,1000)};
 ws.onmessage=e=>{const p=e.data.split(' ');if(p[0]!='s')return;
  mode=+p[1];sel('modes','m',p[1]);sel('exprs','e',p[2]);sel('themes','t',p[3]);
  const n=PH[+p[3]]||PH[0];if(n!=ph){ph=n;faces()}document.documentElement.style.setProperty('--ph',ph);
  if(!touched)$('padhint').textContent=HINT[mode]??'';show()};
}
function send(s){if(ws&&ws.readyState==1)ws.send(s)}
function sel(grp,k,v){for(const el of $(grp).children)el.setAttribute('aria-pressed',el.dataset[k]==v)}
// The cursor is drawn like a phosphor dot: it leaves a fading streak behind it.
let raf=0,life=0,lx=-1,ly=-1;
function fit(){const r=pad.getBoundingClientRect(),d=devicePixelRatio||1;cv.width=r.width*d;cv.height=r.height*d;lx=-1;show()}
function frame(){
 const d=devicePixelRatio||1,px=x/W*cv.width,py=y/W*cv.height,r=(b?8:5)*d;
 g.globalCompositeOperation='destination-out';g.fillStyle=`rgba(0,0,0,${still.matches?1:.2})`;g.fillRect(0,0,cv.width,cv.height);
 g.globalCompositeOperation='source-over';g.shadowColor=ph;g.shadowBlur=14*d;g.strokeStyle=g.fillStyle=ph;g.lineCap='round';
 if(lx>=0&&!still.matches){g.lineWidth=r*1.4;g.beginPath();g.moveTo(lx,ly);g.lineTo(px,py);g.stroke()}
 g.beginPath();g.arc(px,py,r,0,7);g.fill();
 g.shadowBlur=0;g.fillStyle='rgba(255,255,255,.75)';g.beginPath();g.arc(px,py,r*.45,0,7);g.fill();
 lx=px;ly=py;raf=--life>0?requestAnimationFrame(frame):0;
}
function show(){life=36;if(!raf)raf=requestAnimationFrame(frame)}
function moved(){dirty=true;show();if(!touched){touched=true;$('padhint').textContent=''}}
function setBtn(v){if(b!=v){b=v;send(`m ${x|0} ${y|0} ${b}`);show()}}
setInterval(()=>{if(dirty){dirty=false;send(`m ${x|0} ${y|0} ${b}`)}},25);
function absFrom(e){const r=pad.getBoundingClientRect();x=clamp((e.clientX-r.left)/r.width*W);y=clamp((e.clientY-r.top)/r.height*W)}
const locked=()=>document.pointerLockElement===pad;
// mouse: absolute over the screen (or anywhere, if on), relative when captured; touch: like a trackpad
let touch=null;
pad.addEventListener('pointerdown',e=>{
 if(e.pointerType=='touch'){touch={id:e.pointerId,lx:e.clientX,ly:e.clientY,t:Date.now(),d:0};pad.setPointerCapture(e.pointerId);return}
 if(!locked()){absFrom(e);moved();pad.setPointerCapture(e.pointerId)}
 setBtn(1);e.preventDefault();
});
pad.addEventListener('pointerup',e=>{
 if(e.pointerType=='touch'){if(touch&&touch.d<10&&Date.now()-touch.t<300&&!hold){setBtn(1);setTimeout(()=>setBtn(0),60)}touch=null;return}
 if(!hold)setBtn(0);
});
document.addEventListener('pointermove',e=>{
 if(e.pointerType=='touch'){if(!touch||e.pointerId!=touch.id)return;const dx=e.clientX-touch.lx,dy=e.clientY-touch.ly;
  touch.lx=e.clientX;touch.ly=e.clientY;touch.d+=Math.abs(dx)+Math.abs(dy);
  const s=W/pad.getBoundingClientRect().width*1.3;x=clamp(x+dx*s);y=clamp(y+dy*s);moved();return}
 if(locked()){x=clamp(x+e.movementX);y=clamp(y+e.movementY);moved();return}
 if(e.target===pad||pad.contains(e.target)||pad.hasPointerCapture(e.pointerId)){absFrom(e);moved();return}
 if(anywhere){x=clamp(e.clientX/innerWidth*W);y=clamp(e.clientY/innerHeight*W);moved()}
});
document.addEventListener('mousedown',()=>{if(locked())setBtn(1)});
document.addEventListener('mouseup',()=>{if(locked()&&!hold)setBtn(0)});
$('cap').onclick=()=>pad.requestPointerLock();
document.addEventListener('pointerlockchange',()=>{pad.classList.toggle('lock',locked());$('cap').textContent=locked()?'Esc lets go':'Capture mouse'});
$('hold').onclick=()=>{hold=!hold;$('hold').setAttribute('aria-pressed',hold);setBtn(hold?1:0)};
$('anywhere').onclick=()=>{anywhere=!anywhere;$('anywhere').setAttribute('aria-pressed',anywhere)};
const pick=(grp,k,c)=>$(grp).onclick=e=>{const el=e.target.closest('[data-'+k+']');if(el)send(c+' '+el.dataset[k])};
pick('modes','m','M');pick('exprs','e','E');pick('themes','t','T');
addEventListener('resize',fit);$('padhint').textContent=HINT[0];fit();faces();connect();
</script></body></html>)HTML";
