#pragma once

// 轻量中文图传控制台。
// 所有设置通过现有 /control 接口实时修改，不需要额外前端文件系统。
static const char CONTROL_PAGE_HTML[] PROGMEM = R"ESPUI(
<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>ESP32-CAM 图传控制台</title>
<style>
:root{
  color-scheme:dark;
  --bg:#0a0d12;--card:#121824;--card2:#182131;--line:#2a3548;
  --text:#eef4ff;--muted:#93a4bd;--accent:#43b7ff;--ok:#35d07f;
  --warn:#ffbd45;--danger:#ff6679;
}
*{box-sizing:border-box}
body{margin:0;background:linear-gradient(145deg,#080b10,#101827);color:var(--text);
     font-family:system-ui,-apple-system,"Segoe UI","Microsoft YaHei",sans-serif}
header{position:sticky;top:0;z-index:5;display:flex;align-items:center;justify-content:space-between;
       gap:12px;padding:13px 18px;background:#0c111bcc;border-bottom:1px solid var(--line);
       backdrop-filter:blur(12px)}
h1{font-size:18px;margin:0}.sub{font-size:12px;color:var(--muted)}
.status-dot{width:9px;height:9px;border-radius:50%;background:var(--warn);display:inline-block;margin-right:7px}
.layout{display:grid;grid-template-columns:minmax(0,1fr) 350px;gap:14px;padding:14px;max-width:1500px;margin:auto}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;box-shadow:0 14px 40px #0005}
.viewer{min-height:520px;display:flex;align-items:center;justify-content:center;overflow:hidden;
        position:relative;background:#05070a}
#stream{display:block;max-width:100%;max-height:78vh;object-fit:contain;transition:transform .2s}
.empty{color:var(--muted);text-align:center;padding:30px}
.toolbar{display:flex;flex-wrap:wrap;gap:8px;padding:12px;border-top:1px solid var(--line)}
button,.btn{border:1px solid #34445d;background:#1b2739;color:var(--text);border-radius:9px;
            padding:9px 12px;cursor:pointer;font-weight:650}
button:hover{border-color:var(--accent)}button.primary{background:#0878bd;border-color:#159ce9}
button.good{background:#087247;border-color:#18aa70}button.warn{background:#80530c;border-color:#c98519}
.panel{padding:14px;max-height:calc(100vh - 92px);overflow:auto}
.section{padding:12px 0;border-bottom:1px solid var(--line)}.section:last-child{border:0}
.section h2{font-size:14px;margin:0 0 11px}.grid3{display:grid;grid-template-columns:repeat(2,1fr);gap:7px}
.preset{font-size:12px;padding:9px 5px}
.row{display:grid;grid-template-columns:112px 1fr 46px;align-items:center;gap:9px;margin:10px 0}
.row.two{grid-template-columns:112px 1fr}.row label{font-size:13px;color:#d9e5f7}
output{font-variant-numeric:tabular-nums;color:var(--accent);text-align:right;font-size:12px}
select,input[type=range]{width:100%}
select{background:#0d1420;color:var(--text);border:1px solid #334258;border-radius:8px;padding:8px}
input[type=range]{accent-color:var(--accent)}
.switches{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.switch{display:flex;justify-content:space-between;align-items:center;background:var(--card2);
        border:1px solid var(--line);border-radius:9px;padding:8px 10px;font-size:12px}
.switch input{width:18px;height:18px;accent-color:var(--ok)}
.metrics{display:grid;grid-template-columns:1fr 1fr;gap:7px}
.metric{background:#0d1420;border:1px solid var(--line);border-radius:9px;padding:9px}
.metric b{display:block;font-size:14px}.metric span{font-size:11px;color:var(--muted)}
.note{font-size:12px;line-height:1.55;color:var(--muted);margin:8px 0 0}
#toast{position:fixed;left:50%;bottom:22px;transform:translateX(-50%);background:#111b2b;
       border:1px solid #3b4d69;padding:10px 15px;border-radius:10px;opacity:0;pointer-events:none;
       transition:opacity .2s;z-index:10}
#toast.show{opacity:1}
@media(max-width:900px){
  .layout{grid-template-columns:1fr}.viewer{min-height:330px}.panel{max-height:none}
}
</style>
</head>
<body>
<header>
  <div><h1>ESP32-CAM 图传控制台</h1><div class="sub">分辨率、JPEG质量、色彩与延迟实时调节</div></div>
  <div class="sub"><span id="dot" class="status-dot"></span><span id="conn">正在连接</span></div>
</header>

<main class="layout">
  <section class="card">
    <div class="viewer" id="viewer">
      <div class="empty" id="placeholder">点击“开始图传”打开实时画面</div>
      <img id="stream" alt="ESP32-CAM 视频流" hidden>
    </div>
    <div class="toolbar">
      <button class="primary" id="startBtn">开始图传</button>
      <button id="stopBtn">停止图传</button>
      <button id="captureBtn">单张抓拍</button>
      <button id="fullscreenBtn">全屏画面</button>
      <select id="rotate" style="width:auto">
        <option value="0">网页旋转 0°</option>
        <option value="90">网页旋转 90°</option>
        <option value="180">网页旋转 180°</option>
        <option value="270">网页旋转 270°</option>
      </select>
    </div>
  </section>

  <aside class="card panel">
    <section class="section">
      <h2>一键模式</h2>
      <div class="grid3">
        <button class="preset good" data-preset="extreme">极致流畅</button>
        <button class="preset primary" data-preset="motion">小球运动</button>
        <button class="preset" data-preset="fast">普通流畅</button>
        <button class="preset warn" data-preset="clear">清晰观察</button>
      </div>
      <p class="note">默认采用“小球运动”。若仍有卡顿，切换“极致流畅”。</p>
    </section>

    <section class="section">
      <h2>图像质量</h2>
      <div class="row two">
        <label for="framesize">分辨率</label>
        <select id="framesize" data-var="framesize">
          <option value="1">QQVGA 160×120（最高流畅）</option>
          <option value="3">HQVGA 240×176（运动推荐）</option>
          <option value="5">QVGA 320×240</option>
          <option value="6">CIF 400×296</option>
          <option value="7">HVGA 480×320</option>
          <option value="8">VGA 640×480</option>
        </select>
      </div>
      <div class="row">
        <label for="quality">JPEG质量</label>
        <input id="quality" data-var="quality" type="range" min="10" max="63" step="1">
        <output data-for="quality"></output>
      </div>
      <div class="row two">
        <label for="stream_delay">附加帧间隔</label>
        <select id="stream_delay" data-var="stream_delay">
          <option value="0">无额外等待（推荐）</option>
          <option value="20">每帧额外等待 20 ms</option>
          <option value="40">每帧额外等待 40 ms</option>
          <option value="70">每帧额外等待 70 ms</option>
          <option value="120">每帧额外等待 120 ms</option>
        </select>
      </div>
      <p class="note">JPEG质量数字越小越清晰；数字越大压缩越强、通常越流畅。拍小球建议 HQVGA + 质量40～48。</p>
    </section>

    <section class="section">
      <h2>色彩与曝光</h2>
      <div class="row">
        <label for="brightness">亮度</label>
        <input id="brightness" data-var="brightness" type="range" min="-2" max="2" step="1">
        <output data-for="brightness"></output>
      </div>
      <div class="row">
        <label for="contrast">对比度</label>
        <input id="contrast" data-var="contrast" type="range" min="-2" max="2" step="1">
        <output data-for="contrast"></output>
      </div>
      <div class="row">
        <label for="saturation">饱和度</label>
        <input id="saturation" data-var="saturation" type="range" min="-2" max="2" step="1">
        <output data-for="saturation"></output>
      </div>
      <div class="row">
        <label for="ae_level">曝光补偿</label>
        <input id="ae_level" data-var="ae_level" type="range" min="-2" max="2" step="1">
        <output data-for="ae_level"></output>
      </div>
      <div class="switches">
        <label class="switch">自动白平衡<input id="awb" data-var="awb" type="checkbox"></label>
        <label class="switch">白平衡增益<input id="awb_gain" data-var="awb_gain" type="checkbox"></label>
        <label class="switch">自动曝光<input id="aec" data-var="aec" type="checkbox"></label>
        <label class="switch">AEC DSP<input id="aec2" data-var="aec2" type="checkbox"></label>
        <label class="switch">自动增益<input id="agc" data-var="agc" type="checkbox"></label>
        <label class="switch">镜头校正<input id="lenc" data-var="lenc" type="checkbox"></label>
        <label class="switch">Gamma<input id="raw_gma" data-var="raw_gma" type="checkbox"></label>
        <label class="switch">白点校正<input id="wpc" data-var="wpc" type="checkbox"></label>
      </div>
    </section>

    <section class="section">
      <h2>方向与补光</h2>
      <div class="switches">
        <label class="switch">水平镜像<input id="hmirror" data-var="hmirror" type="checkbox"></label>
        <label class="switch">上下翻转<input id="vflip" data-var="vflip" type="checkbox"></label>
      </div>
      <div class="row">
        <label for="led_intensity">补光灯</label>
        <input id="led_intensity" data-var="led_intensity" type="range" min="0" max="255" step="5">
        <output data-for="led_intensity"></output>
      </div>
    </section>

    <section class="section">
      <h2>运行状态</h2>
      <div class="metrics">
        <div class="metric"><b id="rssi">-- dBm</b><span>Wi-Fi信号</span></div>
        <div class="metric"><b id="heap">-- KB</b><span>可用内存</span></div>
        <div class="metric"><b id="psram">-- KB</b><span>可用PSRAM</span></div>
        <div class="metric"><b id="mode">--</b><span>当前画面</span></div>
      </div>
      <p class="note">避免同时打开多个实时流页面。信号低于约 -70 dBm 时，请靠近路由器或热点。</p>
    </section>
  </aside>
</main>
<div id="toast"></div>

<script>
const $ = id => document.getElementById(id);
const streamUrl = `${location.protocol}//${location.hostname}:81/stream`;
let streaming = false;
let toastTimer = null;

function toast(msg){
  const el=$('toast'); el.textContent=msg; el.classList.add('show');
  clearTimeout(toastTimer); toastTimer=setTimeout(()=>el.classList.remove('show'),1500);
}
function setConnected(ok,text){
  $('dot').style.background=ok?'var(--ok)':'var(--danger)';
  $('conn').textContent=text;
}
function updateOutput(el){
  const out=document.querySelector(`output[data-for="${el.id}"]`);
  if(out) out.value=el.value;
}
function startStream(){
  if(streaming) return;
  $('placeholder').hidden=true; $('stream').hidden=false;
  $('stream').src=`${streamUrl}?t=${Date.now()}`;
  streaming=true; toast('实时图传已启动');
}
function stopStream(){
  $('stream').removeAttribute('src'); $('stream').hidden=true;
  $('placeholder').hidden=false; streaming=false; toast('实时图传已停止');
}
async function control(variable,value,quiet=false){
  const r=await fetch(`/control?var=${encodeURIComponent(variable)}&val=${encodeURIComponent(value)}`,{cache:'no-store'});
  if(!r.ok) throw new Error(`${variable} 设置失败`);
  if(!quiet) toast('设置已应用');
}
async function setMany(items){
  const wasStreaming=streaming;
  if(wasStreaming) stopStream();
  try{
    for(const [k,v] of items) await control(k,v,true);
    await refreshStatus();
    toast('模式切换完成');
  }catch(e){toast(e.message);}
  if(wasStreaming) setTimeout(startStream,350);
}
const presets={
  extreme:[['framesize',1],['quality',52],['stream_delay',0],['brightness',0],['contrast',0],['saturation',1]],
  motion:[['framesize',3],['quality',42],['stream_delay',0],['brightness',0],['contrast',0],['saturation',1]],
  fast:[['framesize',5],['quality',35],['stream_delay',0],['brightness',0],['contrast',0],['saturation',1]],
  clear:[['framesize',6],['quality',26],['stream_delay',0],['brightness',0],['contrast',0],['saturation',1]]
};
async function refreshStatus(){
  try{
    const s=await (await fetch('/status',{cache:'no-store'})).json();
    document.querySelectorAll('[data-var]').forEach(el=>{
      const key=el.dataset.var;
      if(s[key]===undefined) return;
      if(el.type==='checkbox') el.checked=!!s[key];
      else el.value=s[key];
      updateOutput(el);
    });
    $('rssi').textContent=`${s.rssi ?? '--'} dBm`;
    $('heap').textContent=`${Math.round((s.free_heap||0)/1024)} KB`;
    $('psram').textContent=`${Math.round((s.free_psram||0)/1024)} KB`;
    const names={1:'QQVGA',3:'HQVGA',5:'QVGA',6:'CIF',7:'HVGA',8:'VGA'};
    $('mode').textContent=`${names[s.framesize]||s.framesize} / Q${s.quality}`;
    setConnected(true,`在线 · ${location.hostname}`);
  }catch(e){setConnected(false,'设备连接失败');}
}

$('startBtn').onclick=startStream;
$('stopBtn').onclick=stopStream;
$('captureBtn').onclick=()=>window.open(`/capture?t=${Date.now()}`,'_blank');
$('fullscreenBtn').onclick=()=>$('viewer').requestFullscreen?.();
$('rotate').onchange=e=>$('stream').style.transform=`rotate(${e.target.value}deg)`;

document.querySelectorAll('.preset').forEach(b=>b.onclick=()=>setMany(presets[b.dataset.preset]));
document.querySelectorAll('input[type=range][data-var]').forEach(el=>{
  el.oninput=()=>updateOutput(el);
  el.onchange=async()=>{
    try{await control(el.dataset.var,el.value);}catch(e){toast(e.message);}
  };
});
document.querySelectorAll('select[data-var]').forEach(el=>{
  el.onchange=async()=>{
    const wasStreaming=streaming;
    if(el.dataset.var==='framesize' && wasStreaming) stopStream();
    try{await control(el.dataset.var,el.value); await refreshStatus();}
    catch(e){toast(e.message);}
    if(el.dataset.var==='framesize' && wasStreaming) setTimeout(startStream,300);
  };
});
document.querySelectorAll('input[type=checkbox][data-var]').forEach(el=>{
  el.onchange=async()=>{
    try{await control(el.dataset.var,el.checked?1:0);}catch(e){toast(e.message);}
  };
});
$('stream').onerror=()=>{setConnected(false,'视频流中断');streaming=false;};
window.addEventListener('beforeunload',stopStream);

refreshStatus().then(()=>setTimeout(startStream,250));
setInterval(refreshStatus,15000);
</script>
</body>
</html>
)ESPUI";
