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
.left-column{min-width:0;display:flex;flex-direction:column;gap:14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;box-shadow:0 14px 40px #0005}
.viewer{min-height:520px;display:flex;align-items:center;justify-content:center;overflow:hidden;
        position:relative;background:#05070a}
#stream{display:block;max-width:100%;max-height:78vh;object-fit:contain;transition:transform .2s}
.empty{color:var(--muted);text-align:center;padding:30px}
.toolbar{display:flex;flex-wrap:wrap;gap:8px;padding:12px;border-top:1px solid var(--line)}
button,.btn{border:1px solid #34445d;background:#1b2739;color:var(--text);border-radius:9px;
            padding:9px 12px;cursor:pointer;font-weight:650}
button:hover{border-color:var(--accent)}
button:disabled,select:disabled,input:disabled{opacity:.48;cursor:not-allowed}
button.primary{background:#0878bd;border-color:#159ce9}
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
.record-state{display:inline-flex;align-items:center;min-height:36px;padding:0 8px;font-size:12px;color:var(--muted)}
.replay-card{padding:14px}.replay-card h2{font-size:15px;margin:0 0 12px}
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
  <div class="left-column">
    <section class="card">
      <div class="viewer" id="viewer">
        <div class="empty" id="placeholder">点击“开始图传”打开实时画面</div>
        <img id="stream"
             alt="ESP32-CAM 视频流"
             crossorigin="anonymous"
             hidden>
      </div>

      <div class="toolbar">
        <button class="primary" id="startBtn">开始图传</button>
        <button id="stopBtn">停止图传</button>
        <button id="captureBtn">单张抓拍</button>
        <button id="fullscreenBtn">全屏画面</button>

        <button class="good" id="recordStartBtn">开始录像</button>
        <button class="warn" id="recordStopBtn" disabled>停止并保存</button>
        <button id="replayBtn" disabled>回放上次录像</button>
        <span id="recordState" class="record-state">未录像</span>

        <select id="rotate" style="width:auto">
          <option value="0">网页旋转 0°</option>
          <option value="90">网页旋转 90°</option>
          <option value="180">网页旋转 180°</option>
          <option value="270">网页旋转 270°</option>
        </select>
      </div>
    </section>

    <!-- 用于把当前唯一的一路 MJPEG 画面转成浏览器可录制的视频流 -->
    <canvas id="recordCanvas" hidden></canvas>

    <section id="replayPanel" class="card replay-card" hidden>
      <h2>录像回放</h2>

      <video id="replayVideo"
             controls
             playsinline
             style="display:block;width:100%;max-height:520px;
                    background:#000;border-radius:10px;">
      </video>

      <div style="margin-top:10px;">
        <a id="saveVideoBtn"
           class="btn"
           download="ball_test.webm">
          下载录像文件
        </a>
      </div>
    </section>
  </div>

  <aside class="card panel">
    <section class="section">
      <h2>一键模式</h2>
      <div class="grid3">
        <button class="preset good" data-preset="extreme">极致流畅</button>
        <button class="preset primary" data-preset="motion">小球运动</button>
        <button class="preset" data-preset="fast">普通流畅</button>
        <button class="preset warn" data-preset="clear">高清观察</button>
      </div>
      <p class="note">
        “高清观察”使用 VGA，但会自动提高 JPEG 压缩率，避免高分辨率帧过大而黑屏。
      </p>
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
        <input id="quality"
               data-var="quality"
               type="range"
               min="10"
               max="63"
               step="1">
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

      <p class="note">
        JPEG 数字越小越清晰、数据越大。切换高分辨率时，网页会自动采用安全的最小压缩值。
      </p>
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
        <!-- 不写 checked；初始状态由 ESP32 的 /status 返回值决定 -->
        <label class="switch">水平镜像<input id="hmirror" data-var="hmirror" type="checkbox"></label>
        <label class="switch">上下翻转<input id="vflip" data-var="vflip" type="checkbox"></label>
      </div>

      <div class="row">
        <label for="led_intensity">补光灯</label>
        <input id="led_intensity"
               data-var="led_intensity"
               type="range"
               min="0"
               max="255"
               step="5">
        <output data-for="led_intensity"></output>
      </div>
    </section>

    <section class="section">
      <h2>运行状态</h2>

      <div class="metrics">
        <div class="metric"><b id="clients">-- 台</b><span>连接客户端</span></div>
        <div class="metric"><b id="heap">-- KB</b><span>可用内存</span></div>
        <div class="metric"><b id="psram">-- KB</b><span>可用PSRAM</span></div>
        <div class="metric"><b id="mode">--</b><span>当前画面</span></div>
      </div>

      <p class="note">
        录像期间会锁定分辨率和图像参数，防止 Canvas 尺寸变化导致录像黑屏或文件损坏。
      </p>
    </section>
  </aside>
</main>
<div id="toast"></div>

<script>
const $ = id => document.getElementById(id);

const streamUrl =
  `${location.protocol}//${location.hostname}:81/stream`;

const FRAME_NAMES = {
  1: 'QQVGA',
  3: 'HQVGA',
  5: 'QVGA',
  6: 'CIF',
  7: 'HVGA',
  8: 'VGA'
};

/*
 * 各分辨率允许的最小 JPEG Quality 数字。
 *
 * 这里的“最小”指数字不能再小：
 * 数字越小，JPEG越清晰、单帧越大，也越容易黑屏或发送超时。
 */
const QUALITY_FLOOR = {
  1: 20,
  3: 24,
  5: 28,
  6: 30,
  7: 34,
  8: 38
};

const PRESETS = {
  extreme: {
    framesize: 1,
    quality: 48,
    stream_delay: 0,
    brightness: 0,
    contrast: 0,
    saturation: 1
  },
  motion: {
    framesize: 3,
    quality: 42,
    stream_delay: 0,
    brightness: 0,
    contrast: 0,
    saturation: 1
  },
  fast: {
    framesize: 5,
    quality: 36,
    stream_delay: 0,
    brightness: 0,
    contrast: 0,
    saturation: 1
  },
  clear: {
    framesize: 8,
    quality: 38,
    stream_delay: 0,
    brightness: 0,
    contrast: 0,
    saturation: 1
  }
};

let streaming = false;
let reconfiguring = false;

let currentFrameSize = 1;
let currentQuality = 30;

let toastTimer = null;
let streamRetryTimer = null;
let streamRetryCount = 0;

// ========================================
// 通用工具
// ========================================
function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function toast(message, duration = 1800) {
  const element = $('toast');

  element.textContent = message;
  element.classList.add('show');

  clearTimeout(toastTimer);

  toastTimer = setTimeout(
    () => element.classList.remove('show'),
    duration
  );
}

function setConnected(ok, text) {
  $('dot').style.background =
    ok ? 'var(--ok)' : 'var(--danger)';

  $('conn').textContent = text;
}

function updateOutput(element) {
  const output = document.querySelector(
    `output[data-for="${element.id}"]`
  );

  if (output) {
    output.value = element.value;
  }
}

function isRecording() {
  return Boolean(
    mediaRecorder &&
    mediaRecorder.state === 'recording'
  );
}

/*
 * 录像和切换分辨率期间锁定摄像头设置。
 */
function setCameraControlsLocked(locked) {
  document
    .querySelectorAll('[data-var], .preset')
    .forEach(element => {
      element.disabled = locked;
    });
}

function getSafeQuality(frameSize, requestedQuality) {
  const floor = QUALITY_FLOOR[frameSize] ?? 30;
  const numericQuality = Number(requestedQuality);

  if (!Number.isFinite(numericQuality)) {
    return floor;
  }

  return Math.max(numericQuality, floor);
}

// ========================================
// 视频流启停
// ========================================
function startStream() {
  if (streaming || reconfiguring) {
    return;
  }

  clearTimeout(streamRetryTimer);

  const image = $('stream');

  image.crossOrigin = 'anonymous';

  $('placeholder').hidden = true;
  image.hidden = false;

  streaming = true;

  image.src =
    `${streamUrl}?t=${Date.now()}`;

  toast('实时图传已启动');
}

function stopStream(force = false) {
  if (isRecording() && !force) {
    toast('请先停止录像，再停止图传');
    return false;
  }

  clearTimeout(streamRetryTimer);

  streaming = false;

  const image = $('stream');

  /*
   * 使用1像素透明图替换MJPEG地址，
   * 促使浏览器关闭端口81的旧TCP连接。
   */
  image.removeAttribute('src');
  image.src =
    'data:image/gif;base64,R0lGODlhAQABAAD/ACwAAAAAAQABAAACADs=';

  image.hidden = true;
  $('placeholder').hidden = false;

  if (!force) {
    toast('实时图传已停止');
  }

  return true;
}

$('stream').onload = () => {
  if (!streaming) {
    return;
  }

  streamRetryCount = 0;

  setConnected(
    true,
    `在线 · ${location.hostname}`
  );
};

$('stream').onerror = () => {
  if (!streaming || reconfiguring) {
    return;
  }

  streaming = false;

  setConnected(false, '视频流中断，正在重连');

  if (streamRetryCount >= 3) {
    toast('视频流连续重连失败，请降低分辨率');
    return;
  }

  streamRetryCount += 1;

  streamRetryTimer = setTimeout(
    () => startStream(),
    900
  );
};

// ========================================
// ESP32控制与状态
// ========================================
async function control(variable, value, quiet = false) {
  const response = await fetch(
    `/control?var=${encodeURIComponent(variable)}` +
    `&val=${encodeURIComponent(value)}`,
    {
      cache: 'no-store'
    }
  );

  if (!response.ok) {
    throw new Error(`${variable} 设置失败`);
  }

  if (!quiet) {
    toast('设置已应用');
  }
}

async function refreshStatus() {
  try {
    const response = await fetch(
      `/status?t=${Date.now()}`,
      {
        cache: 'no-store'
      }
    );

    if (!response.ok) {
      throw new Error('状态接口请求失败');
    }

    const status = await response.json();

    currentFrameSize =
      Number(status.framesize ?? currentFrameSize);

    currentQuality =
      Number(status.quality ?? currentQuality);

    document
      .querySelectorAll('[data-var]')
      .forEach(element => {
        const key = element.dataset.var;

        if (status[key] === undefined) {
          return;
        }

        if (element.type === 'checkbox') {
          element.checked = Boolean(status[key]);
        } else {
          element.value = status[key];
        }

        updateOutput(element);
      });

    $('clients').textContent =
      `${status.clients ?? '--'} 台`;

    $('heap').textContent =
      `${Math.round((status.free_heap || 0) / 1024)} KB`;

    $('psram').textContent =
      `${Math.round((status.free_psram || 0) / 1024)} KB`;

    $('mode').textContent =
      `${FRAME_NAMES[currentFrameSize] || currentFrameSize}` +
      ` / Q${currentQuality}`;

    setConnected(
      true,
      `在线 · ${location.hostname}`
    );

    return status;
  } catch (error) {
    console.error(error);
    setConnected(false, '设备连接失败');
    throw error;
  }
}

// ========================================
// 安全切换分辨率
// ========================================
async function changeFrameSize(
  targetFrameSize,
  requestedQuality = null
) {
  if (isRecording()) {
    toast('请先停止录像，再切换分辨率');
    await refreshStatus().catch(() => {});
    return;
  }

  if (reconfiguring) {
    return;
  }

  const targetSize = Number(targetFrameSize);
  const qualityCandidate =
    requestedQuality ?? $('quality').value;

  const targetQuality =
    getSafeQuality(targetSize, qualityCandidate);

  const wasStreaming = streaming;

  reconfiguring = true;
  setCameraControlsLocked(true);

  try {
    if (wasStreaming) {
      stopStream(true);

      /*
       * 给浏览器和ESP32时间关闭旧的MJPEG TCP连接。
       */
      await sleep(700);
    }

    /*
     * 先提高JPEG压缩，再提高分辨率。
     * 可避免刚切换到VGA时，第一帧仍使用过高画质而过大。
     */
    await control(
      'quality',
      targetQuality,
      true
    );

    await sleep(120);

    await control(
      'framesize',
      targetSize,
      true
    );

    /*
     * 等待OV2640寄存器切换，并产生新尺寸JPEG帧。
     */
    await sleep(900);

    await refreshStatus();

    if (Number(qualityCandidate) < targetQuality) {
      toast(
        `${FRAME_NAMES[targetSize] || targetSize} 为避免黑屏，` +
        `JPEG质量已自动调整为 ${targetQuality}`,
        2600
      );
    } else {
      toast('分辨率切换完成');
    }
  } catch (error) {
    console.error(error);
    toast(`切换失败：${error.message}`, 2600);

    await refreshStatus().catch(() => {});
  } finally {
    reconfiguring = false;
    setCameraControlsLocked(false);

    if (wasStreaming) {
      await sleep(300);
      startStream();
    }
  }
}

// ========================================
// 一键模式
// ========================================
async function applyPreset(name) {
  if (isRecording()) {
    toast('请先停止录像，再切换模式');
    return;
  }

  const preset = PRESETS[name];

  if (!preset || reconfiguring) {
    return;
  }

  const wasStreaming = streaming;

  reconfiguring = true;
  setCameraControlsLocked(true);

  try {
    if (wasStreaming) {
      stopStream(true);
      await sleep(700);
    }

    const safeQuality =
      getSafeQuality(
        preset.framesize,
        preset.quality
      );

    /*
     * 先压缩、后切尺寸，再修改其他参数。
     */
    await control(
      'quality',
      safeQuality,
      true
    );

    await sleep(120);

    await control(
      'framesize',
      preset.framesize,
      true
    );

    for (const [key, value] of Object.entries(preset)) {
      if (
        key === 'quality' ||
        key === 'framesize'
      ) {
        continue;
      }

      await control(key, value, true);
    }

    await sleep(900);
    await refreshStatus();

    toast('模式切换完成');
  } catch (error) {
    console.error(error);
    toast(`模式切换失败：${error.message}`, 2600);

    await refreshStatus().catch(() => {});
  } finally {
    reconfiguring = false;
    setCameraControlsLocked(false);

    if (wasStreaming) {
      await sleep(300);
      startStream();
    }
  }
}

// ========================================
// 页面按钮与参数绑定
// ========================================
$('startBtn').onclick = startStream;

$('stopBtn').onclick = () => {
  stopStream(false);
};

$('captureBtn').onclick = () => {
  window.open(
    `/capture?t=${Date.now()}`,
    '_blank'
  );
};

$('fullscreenBtn').onclick = () => {
  $('viewer').requestFullscreen?.();
};

$('rotate').onchange = event => {
  $('stream').style.transform =
    `rotate(${event.target.value}deg)`;
};

document
  .querySelectorAll('.preset')
  .forEach(button => {
    button.onclick = () =>
      applyPreset(button.dataset.preset);
  });

/*
 * 分辨率单独处理。
 * 不能再让通用 select 监听器重复处理 framesize。
 */
$('framesize').onchange = async event => {
  await changeFrameSize(event.target.value);
};

/*
 * JPEG质量单独处理。
 * 高分辨率时禁止把数字调得过小。
 */
$('quality').oninput = event => {
  updateOutput(event.target);
};

$('quality').onchange = async event => {
  if (isRecording()) {
    toast('录像期间不能修改JPEG质量');
    await refreshStatus().catch(() => {});
    return;
  }

  const requested = Number(event.target.value);
  const safeValue =
    getSafeQuality(currentFrameSize, requested);

  event.target.value = safeValue;
  updateOutput(event.target);

  try {
    await control(
      'quality',
      safeValue,
      true
    );

    await refreshStatus();

    if (requested < safeValue) {
      toast(
        `当前分辨率下最低建议值为 ${safeValue}`,
        2300
      );
    } else {
      toast('JPEG质量已更新');
    }
  } catch (error) {
    toast(error.message);
    await refreshStatus().catch(() => {});
  }
};

/*
 * 其他滑块。
 */
document
  .querySelectorAll(
    'input[type=range][data-var]:not(#quality)'
  )
  .forEach(element => {
    element.oninput = () =>
      updateOutput(element);

    element.onchange = async () => {
      try {
        await control(
          element.dataset.var,
          element.value
        );
      } catch (error) {
        toast(error.message);
        await refreshStatus().catch(() => {});
      }
    };
  });

/*
 * 其他下拉框，明确排除framesize。
 */
document
  .querySelectorAll(
    'select[data-var]:not(#framesize)'
  )
  .forEach(element => {
    element.onchange = async () => {
      try {
        await control(
          element.dataset.var,
          element.value
        );

        await refreshStatus();
      } catch (error) {
        toast(error.message);
        await refreshStatus().catch(() => {});
      }
    };
  });

document
  .querySelectorAll(
    'input[type=checkbox][data-var]'
  )
  .forEach(element => {
    element.onchange = async () => {
      try {
        await control(
          element.dataset.var,
          element.checked ? 1 : 0
        );
      } catch (error) {
        toast(error.message);
        await refreshStatus().catch(() => {});
      }
    };
  });

// ========================================
// 接收端录像与回放
// ========================================
const RECORD_FPS = 15;

let mediaRecorder = null;
let recordedChunks = [];
let recordCanvasStream = null;

let recordDrawTimer = null;
let recordClockTimer = null;
let recordStartedTime = 0;

let lastRecordingUrl = null;
let lastRecordingName = '';

function waitForStreamFrame(timeoutMs = 7000) {
  return new Promise((resolve, reject) => {
    const image = $('stream');
    const startTime = Date.now();

    const timer = setInterval(() => {
      /*
       * MJPEG连接是持续响应，image.complete可能长期为false。
       * 判断naturalWidth/naturalHeight更可靠。
       */
      if (
        image.naturalWidth > 0 &&
        image.naturalHeight > 0
      ) {
        clearInterval(timer);
        resolve();
        return;
      }

      if (Date.now() - startTime > timeoutMs) {
        clearInterval(timer);
        reject(new Error('等待视频画面超时'));
      }
    }, 50);
  });
}

function selectRecorderMimeType() {
  const candidates = [
    'video/webm;codecs=vp8',
    'video/webm;codecs=vp9',
    'video/webm'
  ];

  for (const type of candidates) {
    if (MediaRecorder.isTypeSupported(type)) {
      return type;
    }
  }

  return '';
}

function createRecordingFilename() {
  const now = new Date();

  const pad = value =>
    String(value).padStart(2, '0');

  return (
    'ball_test_' +
    now.getFullYear() +
    pad(now.getMonth() + 1) +
    pad(now.getDate()) +
    '_' +
    pad(now.getHours()) +
    pad(now.getMinutes()) +
    pad(now.getSeconds()) +
    '.webm'
  );
}

function updateRecordingClock() {
  const totalSeconds =
    Math.floor(
      (Date.now() - recordStartedTime) / 1000
    );

  const minutes =
    Math.floor(totalSeconds / 60);

  const seconds =
    totalSeconds % 60;

  $('recordState').textContent =
    `正在录像 ${String(minutes).padStart(2, '0')}:` +
    `${String(seconds).padStart(2, '0')}`;
}

function setRecordingUi(recording) {
  $('recordStartBtn').disabled = recording;
  $('recordStopBtn').disabled = !recording;

  $('startBtn').disabled = recording;
  $('stopBtn').disabled = recording;

  setCameraControlsLocked(recording);
}

async function startRecording() {
  if (isRecording()) {
    return;
  }

  if (!window.MediaRecorder) {
    toast('当前浏览器不支持MediaRecorder');
    return;
  }

  try {
    if (!streaming) {
      startStream();
    }

    await waitForStreamFrame();

    const image = $('stream');
    const canvas = $('recordCanvas');

    canvas.width =
      image.naturalWidth || 240;

    canvas.height =
      image.naturalHeight || 176;

    const context = canvas.getContext(
      '2d',
      {
        alpha: false,
        desynchronized: true
      }
    );

    if (!context) {
      throw new Error('无法创建录像Canvas');
    }

    /*
     * 先绘制第一帧，避免录像开头短暂黑屏。
     */
    context.drawImage(
      image,
      0,
      0,
      canvas.width,
      canvas.height
    );

    recordedChunks = [];

    recordDrawTimer = setInterval(() => {
      if (
        image.naturalWidth > 0 &&
        image.naturalHeight > 0
      ) {
        try {
          context.drawImage(
            image,
            0,
            0,
            canvas.width,
            canvas.height
          );
        } catch (error) {
          console.error(
            'Draw frame failed:',
            error
          );
        }
      }
    }, Math.round(1000 / RECORD_FPS));

    recordCanvasStream =
      canvas.captureStream(RECORD_FPS);

    const mimeType =
      selectRecorderMimeType();

    /*
     * 根据当前分辨率动态设置浏览器录像码率。
     */
    const recordBitrate =
      Math.max(
        400000,
        Math.min(
          2000000,
          canvas.width * canvas.height * 8
        )
      );

    const recorderOptions = {
      videoBitsPerSecond: recordBitrate
    };

    if (mimeType) {
      recorderOptions.mimeType = mimeType;
    }

    mediaRecorder = new MediaRecorder(
      recordCanvasStream,
      recorderOptions
    );

    mediaRecorder.ondataavailable = event => {
      if (
        event.data &&
        event.data.size > 0
      ) {
        recordedChunks.push(event.data);
      }
    };

    mediaRecorder.onerror = event => {
      console.error(
        'MediaRecorder error:',
        event
      );

      toast('录像过程发生错误');
    };

    mediaRecorder.onstop =
      finishRecording;

    mediaRecorder.start(1000);

    recordStartedTime = Date.now();

    recordClockTimer = setInterval(
      updateRecordingClock,
      250
    );

    updateRecordingClock();
    setRecordingUi(true);

    $('replayBtn').disabled = true;

    toast('开始录像');
  } catch (error) {
    console.error(error);

    stopRecordingResources();
    setRecordingUi(false);

    toast(
      `开始录像失败：${error.message}`,
      2800
    );
  }
}

function stopRecording() {
  if (!isRecording()) {
    return;
  }

  $('recordStopBtn').disabled = true;

  $('recordState').textContent =
    '正在生成录像文件……';

  mediaRecorder.stop();
}

function stopRecordingResources() {
  if (recordDrawTimer) {
    clearInterval(recordDrawTimer);
    recordDrawTimer = null;
  }

  if (recordClockTimer) {
    clearInterval(recordClockTimer);
    recordClockTimer = null;
  }

  if (recordCanvasStream) {
    recordCanvasStream
      .getTracks()
      .forEach(track => track.stop());

    recordCanvasStream = null;
  }
}

function finishRecording() {
  stopRecordingResources();
  setRecordingUi(false);

  const mimeType =
    mediaRecorder?.mimeType ||
    'video/webm';

  const recordingBlob = new Blob(
    recordedChunks,
    {
      type: mimeType
    }
  );

  recordedChunks = [];

  if (recordingBlob.size === 0) {
    $('recordState').textContent =
      '录像失败';

    toast('录像文件为空');
    return;
  }

  if (lastRecordingUrl) {
    URL.revokeObjectURL(
      lastRecordingUrl
    );
  }

  lastRecordingUrl =
    URL.createObjectURL(
      recordingBlob
    );

  lastRecordingName =
    createRecordingFilename();

  const replayVideo =
    $('replayVideo');

  replayVideo.src =
    lastRecordingUrl;

  replayVideo.load();

  const saveButton =
    $('saveVideoBtn');

  saveButton.href =
    lastRecordingUrl;

  saveButton.download =
    lastRecordingName;

  $('replayPanel').hidden = false;
  $('replayBtn').disabled = false;

  $('recordState').textContent =
    `录像完成：${lastRecordingName}`;

  toast('录像完成，可以下载或回放');
}

async function replayLastRecording() {
  if (!lastRecordingUrl) {
    toast('目前没有可回放的录像');
    return;
  }

  $('replayPanel').hidden = false;

  const video = $('replayVideo');

  video.currentTime = 0;

  try {
    await video.play();
  } catch (error) {
    console.error(error);
  }

  video.scrollIntoView({
    behavior: 'smooth',
    block: 'center'
  });
}

$('recordStartBtn').onclick =
  startRecording;

$('recordStopBtn').onclick =
  stopRecording;

$('replayBtn').onclick =
  replayLastRecording;

// ========================================
// 页面初始化与退出清理
// ========================================
window.addEventListener(
  'beforeunload',
  () => {
    clearTimeout(streamRetryTimer);

    if (lastRecordingUrl) {
      URL.revokeObjectURL(
        lastRecordingUrl
      );
    }

    stopStream(true);
  }
);

(async () => {
  try {
    await refreshStatus();

    /*
     * main.cpp中 hmirror=0、vflip=0，
     * 因此刷新状态后两个复选框初始均不勾选。
     */
    await sleep(500);
    startStream();
  } catch (error) {
    console.error(error);
  }
})();

setInterval(
  () => {
    if (!reconfiguring) {
      refreshStatus().catch(() => {});
    }
  },
  15000
);
</script>
</body>
</html>
)ESPUI";