#include <Video/WebCameraInput.hpp>

#if defined(__EMSCRIPTEN__) && SCORE_HAS_LIBAV
#include <QDebug>

#include <emscripten/emscripten.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

// clang-format off
EM_JS(void, score_camera_install, (), {
  if(Module.scoreCamera)
    return;

  const S = {
    devices: [],
    generation: 0,
    scanning: false,
    streams: new Map(),
    permissionAsked: false,
    next: 1,
  };
  Module.scoreCamera = S;

  S.scan = function() {
    if(S.scanning)
      return;
    if(!navigator.mediaDevices || !navigator.mediaDevices.enumerateDevices)
    {
      S.generation++;
      return;
    }
    S.scanning = true;
    const done = function(list) {
      S.devices = list.filter(function(d) { return d.kind === 'videoinput'; })
                      .map(function(d) { return {id: d.deviceId, label: d.label}; });
      S.scanning = false;
      S.generation++;
    };
    // enumerateDevices() reports neither ids nor labels until camera permission
    // has been granted, so ask for it here rather than leaving the device list
    // anonymous until playback. The probe stream is released immediately; once
    // permission is remembered this resolves without prompting again.
    const enumerate = function() {
      navigator.mediaDevices.enumerateDevices().then(done).catch(function() { done([]); });
    };
    if(S.permissionAsked || !navigator.mediaDevices.getUserMedia)
    {
      enumerate();
      return;
    }
    S.permissionAsked = true;
    navigator.mediaDevices.getUserMedia({video: true})
      .then(function(stream) {
        stream.getTracks().forEach(function(t) { t.stop(); });
        enumerate();
      })
      .catch(enumerate);
  };

  if(navigator.mediaDevices && navigator.mediaDevices.addEventListener)
    navigator.mediaDevices.addEventListener('devicechange', function() { S.scan(); });
});

EM_JS(void, score_camera_scan, (), {
  Module.scoreCamera.scan();
});

EM_JS(int, score_camera_generation, (), {
  return Module.scoreCamera.generation;
});

EM_JS(int, score_camera_count, (), {
  return Module.scoreCamera.devices.length;
});

EM_JS(void, score_camera_device_id, (int i, char* buf, int cap), {
  const d = Module.scoreCamera.devices[i];
  stringToUTF8(d ? d.id : '', buf, cap);
});

EM_JS(void, score_camera_device_label, (int i, char* buf, int cap), {
  const d = Module.scoreCamera.devices[i];
  stringToUTF8(d ? d.label : '', buf, cap);
});

// Returns a handle > 0. The stream is not usable until score_camera_status
// reports 1: getUserMedia is asynchronous and may be refused.
EM_JS(int, score_camera_open, (const char* deviceId, int w, int h, double fps), {
  const S = Module.scoreCamera;
  if(!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia
     || typeof MediaStreamTrackProcessor === 'undefined')
    return 0;

  const id = UTF8ToString(deviceId);
  const video = {};
  if(id.length > 0)
    video.deviceId = {exact: id};
  if(w > 0)
    video.width = {ideal: w};
  if(h > 0)
    video.height = {ideal: h};
  if(fps > 0)
    video.frameRate = {ideal: fps};

  const handle = S.next++;
  const st = {
    status: 0, w: 0, h: 0, stride: 0, latest: null, free: [],
    closed: false, stream: null, reader: null, error: '',
  };
  S.streams.set(handle, st);

  const fail = function(e) {
    st.error = (e && e.name ? e.name : 'error') + ': ' + (e && e.message ? e.message : '');
    st.status = -1;
  };

  navigator.mediaDevices.getUserMedia({video: video, audio: false}).then(function(stream) {
    if(st.closed)
    {
      stream.getTracks().forEach(function(t) { t.stop(); });
      return;
    }
    st.stream = stream;
    const track = stream.getVideoTracks()[0];
    if(!track)
    {
      fail({name: 'NoTrack', message: 'stream has no video track'});
      return;
    }
    st.reader = new MediaStreamTrackProcessor({track: track}).readable.getReader();
    st.status = 1;

    // One read in flight at a time: MediaStreamTrackProcessor drops what we do
    // not keep up with, which is what a live camera wants.
    const pump = function() {
      if(st.closed)
        return;
      st.reader.read().then(function(res) {
        if(st.closed || res.done)
          return;
        const frame = res.value;
        const fw = frame.displayWidth, fh = frame.displayHeight;
        let size = 0;
        try {
          size = frame.allocationSize({format: 'RGBA'});
        } catch(e) {
          frame.close();
          fail(e);
          return;
        }
        let buf = null;
        while(st.free.length > 0)
        {
          const b = st.free.pop();
          if(b.byteLength === size) { buf = b; break; }
        }
        if(!buf)
          buf = new Uint8Array(size);
        frame.copyTo(buf, {format: 'RGBA'}).then(function(layout) {
          frame.close();
          if(st.closed)
            return;
          st.w = fw;
          st.h = fh;
          st.stride = layout && layout.length > 0 ? layout[0].stride : fw * 4;
          if(st.latest)
            st.free.push(st.latest);
          st.latest = buf;
          pump();
        }).catch(function(e) {
          frame.close();
          fail(e);
        });
      }).catch(fail);
    };
    pump();
  }).catch(fail);

  return handle;
});

// 0: opening, 1: running, -1: failed, -2: unknown handle
EM_JS(int, score_camera_status, (int handle), {
  const st = Module.scoreCamera.streams.get(handle);
  return st ? st.status : -2;
});

EM_JS(void, score_camera_error, (int handle, char* buf, int cap), {
  const st = Module.scoreCamera.streams.get(handle);
  stringToUTF8(st ? st.error : '', buf, cap);
});

// Writes {width, height, stride, bytes} into `out` when a frame is waiting.
// Copies it into `dst` and consumes it if `cap` is large enough, otherwise
// leaves it pending and returns -1 so the caller can size its buffer.
// Returns the number of bytes copied, 0 if no frame is pending.
EM_JS(int, score_camera_take, (int handle, int* out, void* dst, int cap), {
  const st = Module.scoreCamera.streams.get(handle);
  if(!st || !st.latest)
    return 0;

  const buf = st.latest;
  const o = out >> 2;
  HEAP32[o] = st.w;
  HEAP32[o + 1] = st.h;
  HEAP32[o + 2] = st.stride;
  HEAP32[o + 3] = buf.byteLength;
  if(buf.byteLength > cap)
    return -1;

  HEAPU8.set(buf, dst);
  st.latest = null;
  st.free.push(buf);
  return buf.byteLength;
});

EM_JS(void, score_camera_close, (int handle), {
  const S = Module.scoreCamera;
  const st = S.streams.get(handle);
  if(!st)
    return;
  st.closed = true;
  if(st.reader)
    try { st.reader.cancel(); } catch(e) { }
  if(st.stream)
    st.stream.getTracks().forEach(function(t) { t.stop(); });
  st.latest = null;
  st.free = [];
  S.streams.delete(handle);
});
// clang-format on

namespace Video
{
namespace
{
struct WebCameraJS
{
  WebCameraJS() { score_camera_install(); }
};

void ensureInstalled()
{
  static const WebCameraJS js;
}
}

void webCameraScan() noexcept
{
  ensureInstalled();
  score_camera_scan();
}

int webCameraScanGeneration() noexcept
{
  ensureInstalled();
  return score_camera_generation();
}

std::vector<WebCameraDevice> webCameraDevices() noexcept
{
  ensureInstalled();
  const int n = score_camera_count();
  std::vector<WebCameraDevice> devices;
  devices.reserve(n);
  char id[256], label[256];
  for(int i = 0; i < n; i++)
  {
    id[0] = 0;
    label[0] = 0;
    score_camera_device_id(i, id, sizeof(id));
    score_camera_device_label(i, label, sizeof(label));
    devices.push_back({id, label});
  }
  return devices;
}

WebCameraInput::WebCameraInput() noexcept
{
  ensureInstalled();
  realTime = true;
  pixel_format = AV_PIX_FMT_RGBA;
}

WebCameraInput::~WebCameraInput() noexcept
{
  stop();
}

bool WebCameraInput::load(
    const std::string& deviceId, int w, int h, double fps) noexcept
{
  stop();
  m_deviceId = deviceId;
  m_requestedWidth = w;
  m_requestedHeight = h;
  m_requestedFps = fps;
  this->width = w;
  this->height = h;
  this->fps = fps;
  return true;
}

bool WebCameraInput::start() noexcept
{
  if(m_handle != 0)
    return false;

  m_reportedError = false;
  m_handle = score_camera_open(
      m_deviceId.c_str(), m_requestedWidth, m_requestedHeight, m_requestedFps);
  if(m_handle == 0)
  {
    qDebug() << "score: no getUserMedia / MediaStreamTrackProcessor in this browser";
    return false;
  }
  return true;
}

void WebCameraInput::stop() noexcept
{
  if(m_handle != 0)
  {
    score_camera_close(m_handle);
    m_handle = 0;
  }
  m_frames.drain();
}

void WebCameraInput::fetch() noexcept
{
  if(m_handle == 0)
    return;

  if(score_camera_status(m_handle) < 0)
  {
    if(!m_reportedError)
    {
      m_reportedError = true;
      char err[512];
      err[0] = 0;
      score_camera_error(m_handle, err, sizeof(err));
      qDebug() << "score: camera failed:" << err;
    }
    return;
  }

  int info[4] = {};
  if(score_camera_take(m_handle, info, nullptr, 0) != -1)
    return;

  const int w = info[0], h = info[1], stride = info[2], bytes = info[3];
  if(w <= 0 || h <= 0 || bytes <= 0)
    return;

  auto frame = m_frames.newFrame();
  auto storage = initFrameBuffer(*frame, bytes);
  if(score_camera_take(m_handle, info, storage, bytes) <= 0)
  {
    m_frames.enqueue_decoding_error(frame.release());
    return;
  }

  frame->format = AV_PIX_FMT_RGBA;
  frame->width = w;
  frame->height = h;
  frame->linesize[0] = stride;
  frame->data[0] = storage;

  this->width = w;
  this->height = h;
  this->pixel_format = AV_PIX_FMT_RGBA;

  m_frames.enqueue(frame.release());
}

AVFrame* WebCameraInput::dequeue_frame() noexcept
{
  fetch();
  return m_frames.dequeue();
}

void WebCameraInput::release_frame(AVFrame* frame) noexcept
{
  m_frames.release(frame);
}
}
#endif
