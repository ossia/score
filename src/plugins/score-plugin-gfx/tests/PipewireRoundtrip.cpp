// PipeWire video round-trip test harness.
//
// Validates score's PipeWire video I/O (Gfx/Pipewire) end-to-end against the
// live PipeWire daemon, exhaustively across pixel formats and transports:
//
//   A. score -> score:   TexgenNode -> PipewireOutputNode ~~pw~~ InputStream
//      (sysmem SHM on GL+Vulkan; EGL/GBM DMA-BUF on GL)
//   B. raw pw producer -> score InputStream: a video-src.c-style libpipewire
//      producer paints the test signal in EVERY input format score claims to
//      support (RGBA/BGRA/10-bit/F16/RGB24/I420/YV12/NV12/P010/YUY2/UYVY),
//      validating each sysmem copy path + negotiation fallback.
//
// Verification mirrors tests/AJARoundtrip.cpp: a rolling 6-bit frame index in
// a top band (robust to 4:2:0/limited-range) + PSNR of a gradient field,
// converted to RGBA via swscale (or DRM-PRIME dmabuf mmap for zero-copy
// input). Matrix report at the end; exit 1 on unexpected FAIL.

#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/interop/DrmFourcc.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/TexgenNode.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Pipewire/PipewireInputDevice.hpp>
#include <Gfx/Pipewire/PipewireOutputDevice.hpp>

#include <Video/ExternalInput.hpp>


extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>

#include <sys/mman.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

#include <map>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace
{
// ---------------------------------------------------------------------------
// Test signal (same convention as AJARoundtrip): 6-bit rolling index encoded
// as 2 bits per RGB channel in a top band, gradient field below.
// ---------------------------------------------------------------------------
constexpr int kIdxMod = 64;
inline uint8_t lvl(int b) { return uint8_t(32 + 64 * (b & 0x3)); }
inline int unlvl(uint8_t v) { return std::clamp((int(v) - 32 + 32) / 64, 0, 3); }

void paint(uint8_t* rgba, int w, int h, int idx)
{
  const int band = std::max(1, h / 8);
  const uint8_t br = lvl(idx & 0x3), bg = lvl((idx >> 2) & 0x3),
                bb = lvl((idx >> 4) & 0x3);
  for(int y = 0; y < h; ++y)
  {
    uint8_t* row = rgba + size_t(y) * w * 4;
    if(y < band)
      for(int x = 0; x < w; ++x)
      {
        row[x * 4 + 0] = br;
        row[x * 4 + 1] = bg;
        row[x * 4 + 2] = bb;
        row[x * 4 + 3] = 255;
      }
    else
    {
      const uint8_t gy = uint8_t((y * 255) / h);
      for(int x = 0; x < w; ++x)
      {
        row[x * 4 + 0] = uint8_t((x * 255) / w);
        row[x * 4 + 1] = gy;
        row[x * 4 + 2] = 128;
        row[x * 4 + 3] = 255;
      }
    }
  }
}

std::array<std::atomic<int64_t>, kIdxMod> g_sendNs{};
inline int64_t nowNs()
{
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
void g_paint(unsigned char* rgb, int width, int height, int t)
{
  const int idx = t % kIdxMod;
  g_sendNs[idx].store(nowNs(), std::memory_order_relaxed);
  paint(rgb, width, height, idx);
}

int idxFromRgba(const uint8_t* rgba, int w, int h)
{
  const int band = std::max(1, h / 8);
  const int y = band / 2;
  long sr = 0, sg = 0, sb = 0, n = 0;
  for(int x = w / 4; x < 3 * w / 4; x += w / 32 + 1)
  {
    const uint8_t* p = rgba + (size_t(y) * w + x) * 4;
    sr += p[0];
    sg += p[1];
    sb += p[2];
    ++n;
  }
  if(!n)
    return -1;
  return unlvl(uint8_t(sr / n)) | (unlvl(uint8_t(sg / n)) << 2)
         | (unlvl(uint8_t(sb / n)) << 4);
}

double psnrGradient(const uint8_t* recv, const uint8_t* ref, int w, int h)
{
  const int band = std::max(1, h / 8);
  double mse = 0;
  long n = 0;
  for(int y = band; y < h; ++y)
    for(int x = 0; x < w; ++x)
      for(int c = 0; c < 3; ++c)
      {
        const double d = double(recv[(size_t(y) * w + x) * 4 + c])
                         - ref[(size_t(y) * w + x) * 4 + c];
        mse += d * d;
        ++n;
      }
  if(!n)
    return 0;
  mse /= n;
  return mse <= 1e-9 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);
}

// ---------------------------------------------------------------------------
// AVFrame -> packed RGBA via swscale. Handles every sysmem format the input
// device can produce. DRM_PRIME frames are converted by mmapping the (LINEAR)
// dmabuf and wrapping the mapping as a plain AVFrame first.
// ---------------------------------------------------------------------------
// Shared with the production code — the one authoritative table.
using score::gfx::interop::drmFourccToAv;

bool frameToRgba(const AVFrame* f, std::vector<uint8_t>& rgba)
{
  const int w = f->width, h = f->height;
  rgba.resize(size_t(w) * h * 4);

  const AVFrame* src = f;
  AVFrame tmp{};
  void* mapped = nullptr;
  size_t mappedSize = 0;

  if(f->format == AV_PIX_FMT_DRM_PRIME)
  {
    const auto* d = reinterpret_cast<const AVDRMFrameDescriptor*>(f->data[0]);
    if(!d || d->nb_objects < 1 || d->nb_layers < 1)
      return false;
    const auto av = drmFourccToAv(d->layers[0].format);
    if(av == AV_PIX_FMT_NONE)
      return false; // non-RGBA dmabuf: extend when needed
    mappedSize = d->objects[0].size;
    mapped = mmap(nullptr, mappedSize, PROT_READ, MAP_SHARED, d->objects[0].fd, 0);
    if(mapped == MAP_FAILED)
      return false;
    tmp.format = av;
    tmp.width = w;
    tmp.height = h;
    tmp.data[0]
        = static_cast<uint8_t*>(mapped) + d->layers[0].planes[0].offset;
    tmp.linesize[0] = int(d->layers[0].planes[0].pitch);
    src = &tmp;
  }

  SwsContext* sws = sws_getContext(
      w, h, AVPixelFormat(src->format), w, h, AV_PIX_FMT_RGBA, SWS_POINT,
      nullptr, nullptr, nullptr);
  bool ok = false;
  if(sws)
  {
    uint8_t* dst[4] = {rgba.data(), nullptr, nullptr, nullptr};
    int dstStride[4] = {w * 4, 0, 0, 0};
    ok = sws_scale(sws, src->data, src->linesize, 0, h, dst, dstStride) == h;
    sws_freeContext(sws);
  }
  if(mapped)
    munmap(mapped, mappedSize);
  return ok;
}

// ---------------------------------------------------------------------------
// Per-cell verification metrics.
// ---------------------------------------------------------------------------
struct Metrics
{
  int frames = 0, gaps = 0, repeats = 0, psnrN = 0, badConvert = 0;
  double psnrSum = 0, psnrMin = 99.0;
  int lastIdx = -1;
  int64_t startNs = 0;
  double latSumMs = 0;
  int latN = 0;
  bool drmPrime = false;
  static constexpr int64_t kWarmupNs = 700'000'000;

  void onFrame(const AVFrame* f, std::vector<uint8_t>& rgba)
  {
    const int64_t t = nowNs();
    ++frames;
    if(f->format == AV_PIX_FMT_DRM_PRIME)
      drmPrime = true;
    const bool warm = (t - startNs) < kWarmupNs;
    if(!frameToRgba(f, rgba))
    {
      ++badConvert;
      return;
    }
    // Debug: SCORE_PWRT_DUMP=<prefix> dumps the 30th converted frame.
    static const QByteArray dumpPrefix = qgetenv("SCORE_PWRT_DUMP");
    if(!dumpPrefix.isEmpty() && frames == 30)
      QImage(rgba.data(), f->width, f->height, QImage::Format_RGBA8888)
          .save(QString::fromUtf8(dumpPrefix) + ".png");
    const int idx = idxFromRgba(rgba.data(), f->width, f->height);
    if(idx >= 0 && lastIdx >= 0 && !warm)
    {
      const int step = ((idx - lastIdx) % kIdxMod + kIdxMod) % kIdxMod;
      if(step == 0)
        ++repeats;
      else if(step > 1)
        gaps += step - 1;
    }
    lastIdx = idx;
    if(!warm && idx >= 0)
    {
      const int64_t sent = g_sendNs[idx].load(std::memory_order_relaxed);
      if(sent > 0 && t >= sent)
      {
        latSumMs += (t - sent) / 1e6;
        ++latN;
      }
      if(frames % 4 == 0)
      {
        std::vector<uint8_t> ref(size_t(f->width) * f->height * 4);
        paint(ref.data(), f->width, f->height, idx);
        const double p
            = psnrGradient(rgba.data(), ref.data(), f->width, f->height);
        psnrSum += p;
        psnrMin = std::min(psnrMin, p);
        ++psnrN;
      }
    }
  }
};

struct Result
{
  std::string cell, transport, status;
  std::string wire; // consumer-negotiated transport ("shm"/"dmabuf"/"none")
  int recv = 0, gaps = 0, repeats = 0, badConvert = 0;
  int sent = 0; // buffers the producer handed to pipewire
  double fps = 0, minPsnr = 0, meanLatMs = 0;
  bool drmPrime = false;
};

// ---------------------------------------------------------------------------
// Raw libpipewire test-signal producer (modeled on src/examples/video-src.c):
// paints the index band + gradient in an arbitrary SPA video format so the
// score input path can be validated for formats score can't emit itself.
// ---------------------------------------------------------------------------
struct RgbToYuv
{
  // BT.709 limited range.
  static void px(const uint8_t* rgb, uint8_t& Y, uint8_t& U, uint8_t& V)
  {
    const double r = rgb[0], g = rgb[1], b = rgb[2];
    Y = uint8_t(std::clamp(16.0 + (46.742 * r + 157.243 * g + 15.874 * b) / 255.0, 0., 255.));
    U = uint8_t(std::clamp(128.0 + (-25.765 * r - 86.674 * g + 112.439 * b) / 255.0, 0., 255.));
    V = uint8_t(std::clamp(128.0 + (112.439 * r - 102.129 * g - 10.310 * b) / 255.0, 0., 255.));
  }
};

struct PwFormat
{
  const char* name;      // CLI / matrix name
  uint32_t spa;          // SPA_VIDEO_FORMAT_*
  const char* urlFormat; // score input URL format= token
  int blocks;            // spa_data planes for SHM (one packed buffer)
  // fills one frame; stride = bytes per row of plane 0
  void (*fill)(uint8_t* base, int w, int h, int stride, int idx);
};

// --- packed RGB fills ---
void fillRgba(uint8_t* d, int w, int h, int stride, int idx)
{
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  paint(rgba.data(), w, h, idx);
  for(int y = 0; y < h; ++y)
    memcpy(d + size_t(y) * stride, rgba.data() + size_t(y) * w * 4, w * 4);
}
void fillBgra(uint8_t* d, int w, int h, int stride, int idx)
{
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  paint(rgba.data(), w, h, idx);
  for(int y = 0; y < h; ++y)
  {
    uint8_t* row = d + size_t(y) * stride;
    const uint8_t* s = rgba.data() + size_t(y) * w * 4;
    for(int x = 0; x < w; ++x)
    {
      row[x * 4 + 0] = s[x * 4 + 2];
      row[x * 4 + 1] = s[x * 4 + 1];
      row[x * 4 + 2] = s[x * 4 + 0];
      row[x * 4 + 3] = s[x * 4 + 3];
    }
  }
}
void fillRgb24(uint8_t* d, int w, int h, int stride, int idx)
{
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  paint(rgba.data(), w, h, idx);
  for(int y = 0; y < h; ++y)
  {
    uint8_t* row = d + size_t(y) * stride;
    const uint8_t* s = rgba.data() + size_t(y) * w * 4;
    for(int x = 0; x < w; ++x)
    {
      row[x * 3 + 0] = s[x * 4 + 0];
      row[x * 3 + 1] = s[x * 4 + 1];
      row[x * 3 + 2] = s[x * 4 + 2];
    }
  }
}
// xRGB_210LE: 32-bit word, X[31:30] R[29:20] G[19:10] B[9:0] (LE)
void fillXrgb210(uint8_t* d, int w, int h, int stride, int idx)
{
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  paint(rgba.data(), w, h, idx);
  for(int y = 0; y < h; ++y)
  {
    uint32_t* row = reinterpret_cast<uint32_t*>(d + size_t(y) * stride);
    const uint8_t* s = rgba.data() + size_t(y) * w * 4;
    for(int x = 0; x < w; ++x)
    {
      const uint32_t r = uint32_t(s[x * 4 + 0]) << 2, g = uint32_t(s[x * 4 + 1]) << 2,
                     b = uint32_t(s[x * 4 + 2]) << 2;
      row[x] = (3u << 30) | (r << 20) | (g << 10) | b;
    }
  }
}
void fillXbgr210(uint8_t* d, int w, int h, int stride, int idx)
{
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  paint(rgba.data(), w, h, idx);
  for(int y = 0; y < h; ++y)
  {
    uint32_t* row = reinterpret_cast<uint32_t*>(d + size_t(y) * stride);
    const uint8_t* s = rgba.data() + size_t(y) * w * 4;
    for(int x = 0; x < w; ++x)
    {
      const uint32_t r = uint32_t(s[x * 4 + 0]) << 2, g = uint32_t(s[x * 4 + 1]) << 2,
                     b = uint32_t(s[x * 4 + 2]) << 2;
      row[x] = (3u << 30) | (b << 20) | (g << 10) | r;
    }
  }
}
// RGBA_F16: half floats. Minimal float->half (values in [0,1], no
// denormal/inf concerns).
uint16_t f2h(float f)
{
  uint32_t x;
  memcpy(&x, &f, 4);
  const uint32_t sign = (x >> 16) & 0x8000;
  int32_t exp = int32_t((x >> 23) & 0xFF) - 127 + 15;
  uint32_t mant = (x >> 13) & 0x3FF;
  if(exp <= 0)
    return uint16_t(sign);
  if(exp >= 31)
    return uint16_t(sign | 0x7C00);
  return uint16_t(sign | (uint32_t(exp) << 10) | mant);
}
void fillRgbaF16(uint8_t* d, int w, int h, int stride, int idx)
{
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  paint(rgba.data(), w, h, idx);
  for(int y = 0; y < h; ++y)
  {
    uint16_t* row = reinterpret_cast<uint16_t*>(d + size_t(y) * stride);
    const uint8_t* s = rgba.data() + size_t(y) * w * 4;
    for(int x = 0; x < w; ++x)
      for(int c = 0; c < 4; ++c)
        row[x * 4 + c] = f2h(s[x * 4 + c] / 255.0f);
  }
}
// --- YUV fills (all take plane-0 stride; planes packed sequentially with
// their natural strides, matching how the SHM buffer is sized) ---
void fillI420(uint8_t* d, int w, int h, int stride, int idx)
{
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  paint(rgba.data(), w, h, idx);
  uint8_t* Y = d;
  uint8_t* U = d + size_t(stride) * h;
  uint8_t* V = U + size_t(stride / 2) * (h / 2);
  for(int y = 0; y < h; ++y)
    for(int x = 0; x < w; ++x)
    {
      uint8_t yy, uu, vv;
      RgbToYuv::px(&rgba[(size_t(y) * w + x) * 4], yy, uu, vv);
      Y[size_t(y) * stride + x] = yy;
      if((x & 1) == 0 && (y & 1) == 0)
      {
        U[size_t(y / 2) * (stride / 2) + x / 2] = uu;
        V[size_t(y / 2) * (stride / 2) + x / 2] = vv;
      }
    }
}
void fillYv12(uint8_t* d, int w, int h, int stride, int idx)
{
  // Same as I420 but V plane first.
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  paint(rgba.data(), w, h, idx);
  uint8_t* Y = d;
  uint8_t* V = d + size_t(stride) * h;
  uint8_t* U = V + size_t(stride / 2) * (h / 2);
  for(int y = 0; y < h; ++y)
    for(int x = 0; x < w; ++x)
    {
      uint8_t yy, uu, vv;
      RgbToYuv::px(&rgba[(size_t(y) * w + x) * 4], yy, uu, vv);
      Y[size_t(y) * stride + x] = yy;
      if((x & 1) == 0 && (y & 1) == 0)
      {
        U[size_t(y / 2) * (stride / 2) + x / 2] = uu;
        V[size_t(y / 2) * (stride / 2) + x / 2] = vv;
      }
    }
}
void fillNv12(uint8_t* d, int w, int h, int stride, int idx)
{
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  paint(rgba.data(), w, h, idx);
  uint8_t* Y = d;
  uint8_t* UV = d + size_t(stride) * h;
  for(int y = 0; y < h; ++y)
    for(int x = 0; x < w; ++x)
    {
      uint8_t yy, uu, vv;
      RgbToYuv::px(&rgba[(size_t(y) * w + x) * 4], yy, uu, vv);
      Y[size_t(y) * stride + x] = yy;
      if((x & 1) == 0 && (y & 1) == 0)
      {
        UV[size_t(y / 2) * stride + x + 0] = uu;
        UV[size_t(y / 2) * stride + x + 1] = vv;
      }
    }
}
void fillP010(uint8_t* d, int w, int h, int stride, int idx)
{
  // stride is plane-0 stride in BYTES (w*2).
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  paint(rgba.data(), w, h, idx);
  uint16_t* Y = reinterpret_cast<uint16_t*>(d);
  uint16_t* UV = reinterpret_cast<uint16_t*>(d + size_t(stride) * h);
  const int sw = stride / 2;
  for(int y = 0; y < h; ++y)
    for(int x = 0; x < w; ++x)
    {
      uint8_t yy, uu, vv;
      RgbToYuv::px(&rgba[(size_t(y) * w + x) * 4], yy, uu, vv);
      Y[size_t(y) * sw + x] = uint16_t(yy) << 8; // 10-bit in high bits
      if((x & 1) == 0 && (y & 1) == 0)
      {
        UV[size_t(y / 2) * sw + x + 0] = uint16_t(uu) << 8;
        UV[size_t(y / 2) * sw + x + 1] = uint16_t(vv) << 8;
      }
    }
}
void fillYuy2(uint8_t* d, int w, int h, int stride, int idx)
{
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  paint(rgba.data(), w, h, idx);
  for(int y = 0; y < h; ++y)
  {
    uint8_t* row = d + size_t(y) * stride;
    for(int x = 0; x < w; x += 2)
    {
      uint8_t y0, u0, v0, y1, u1, v1;
      RgbToYuv::px(&rgba[(size_t(y) * w + x) * 4], y0, u0, v0);
      RgbToYuv::px(&rgba[(size_t(y) * w + std::min(x + 1, w - 1)) * 4], y1, u1, v1);
      row[x * 2 + 0] = y0;
      row[x * 2 + 1] = uint8_t((u0 + u1) / 2);
      row[x * 2 + 2] = y1;
      row[x * 2 + 3] = uint8_t((v0 + v1) / 2);
    }
  }
}
void fillUyvy(uint8_t* d, int w, int h, int stride, int idx)
{
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  paint(rgba.data(), w, h, idx);
  for(int y = 0; y < h; ++y)
  {
    uint8_t* row = d + size_t(y) * stride;
    for(int x = 0; x < w; x += 2)
    {
      uint8_t y0, u0, v0, y1, u1, v1;
      RgbToYuv::px(&rgba[(size_t(y) * w + x) * 4], y0, u0, v0);
      RgbToYuv::px(&rgba[(size_t(y) * w + std::min(x + 1, w - 1)) * 4], y1, u1, v1);
      row[x * 2 + 0] = uint8_t((u0 + u1) / 2);
      row[x * 2 + 1] = y0;
      row[x * 2 + 2] = uint8_t((v0 + v1) / 2);
      row[x * 2 + 3] = y1;
    }
  }
}

// Bytes for the whole frame given plane-0 stride.
size_t frameBytesFor(uint32_t spa, int w, int h, int stride)
{
  switch(spa)
  {
    case SPA_VIDEO_FORMAT_RGB:
      return size_t(stride) * h;
    case SPA_VIDEO_FORMAT_I420:
    case SPA_VIDEO_FORMAT_YV12:
      return size_t(stride) * h + 2 * size_t(stride / 2) * (h / 2);
    case SPA_VIDEO_FORMAT_NV12:
      return size_t(stride) * h + size_t(stride) * (h / 2);
    case SPA_VIDEO_FORMAT_P010_10LE:
      return size_t(stride) * h + size_t(stride) * (h / 2);
    default:
      return size_t(stride) * h; // packed single-plane
  }
}

int strideFor(uint32_t spa, int w)
{
  switch(spa)
  {
    case SPA_VIDEO_FORMAT_RGB:
      return SPA_ROUND_UP_N(w * 3, 4);
    case SPA_VIDEO_FORMAT_I420:
    case SPA_VIDEO_FORMAT_YV12:
    case SPA_VIDEO_FORMAT_NV12:
      return w;
    case SPA_VIDEO_FORMAT_P010_10LE:
    case SPA_VIDEO_FORMAT_YUY2:
    case SPA_VIDEO_FORMAT_UYVY:
      return w * 2;
    case SPA_VIDEO_FORMAT_RGBA_F16:
      return w * 8;
    default:
      return w * 4;
  }
}

const std::vector<PwFormat>& pwFormats()
{
  static const std::vector<PwFormat> t = {
      {"rgba", SPA_VIDEO_FORMAT_RGBA, "rgba", 1, fillRgba},
      {"bgra", SPA_VIDEO_FORMAT_BGRA, "bgra", 1, fillBgra},
      {"rgb10a2", SPA_VIDEO_FORMAT_xRGB_210LE, "rgb10a2", 1, fillXrgb210},
      {"bgr10a2", SPA_VIDEO_FORMAT_xBGR_210LE, "bgr10a2", 1, fillXbgr210},
      {"rgba16f", SPA_VIDEO_FORMAT_RGBA_F16, "rgba16f", 1, fillRgbaF16},
      {"rgb24", SPA_VIDEO_FORMAT_RGB, "rgb", 1, fillRgb24},
      {"yuv420p", SPA_VIDEO_FORMAT_I420, "yuv420p", 1, fillI420},
      {"yv12", SPA_VIDEO_FORMAT_YV12, "yv12", 1, fillYv12},
      {"nv12", SPA_VIDEO_FORMAT_NV12, "nv12", 1, fillNv12},
      {"p010", SPA_VIDEO_FORMAT_P010_10LE, "p010", 1, fillP010},
      {"yuyv422", SPA_VIDEO_FORMAT_YUY2, "yuyv422", 1, fillYuy2},
      {"uyvy422", SPA_VIDEO_FORMAT_UYVY, "uyvy422", 1, fillUyvy},
  };
  return t;
}

struct RawProducer
{
  pw_thread_loop* loop{};
  pw_stream* stream{};
  spa_hook listener{};
  spa_source* timer{};
  const PwFormat* fmt{};
  int w = 0, h = 0, stride = 0, extraStride = 0;
  double fps = 30.;
  std::atomic<int> counter{0};
  std::atomic<int> queued{0};
  std::string nodeName;

  static void on_process_cb(void* ud) { static_cast<RawProducer*>(ud)->produce(); }
  static void on_timer_cb(void* ud, uint64_t)
  {
    auto* self = static_cast<RawProducer*>(ud);
    pw_stream_trigger_process(self->stream);
  }
  // Announce the buffer requirements once the format lands: otherwise the
  // server sizes buffers from the format's natural stride and a padded
  // stride overruns them.
  static void on_param_changed_cb(void* ud, uint32_t id, const spa_pod* param)
  {
    auto* self = static_cast<RawProducer*>(ud);
    if(!param || id != SPA_PARAM_Format)
      return;
    uint8_t pod[512];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(pod, sizeof(pod));
    const uint32_t size
        = uint32_t(frameBytesFor(self->fmt->spa, self->w, self->h, self->stride));
    const spa_pod* params[1];
    params[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(
        &b, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
        SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(4, 2, 8),
        SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1), SPA_PARAM_BUFFERS_size,
        SPA_POD_Int(int(size)), SPA_PARAM_BUFFERS_stride,
        SPA_POD_Int(self->stride), SPA_PARAM_BUFFERS_dataType,
        SPA_POD_CHOICE_FLAGS_Int(
            (1 << SPA_DATA_MemPtr) | (1 << SPA_DATA_MemFd))));
    pw_stream_update_params(self->stream, params, 1);
  }

  void produce()
  {
    pw_buffer* b = pw_stream_dequeue_buffer(stream);
    if(!b)
      return;
    spa_buffer* buf = b->buffer;
    const size_t need = frameBytesFor(fmt->spa, w, h, stride);
    if(buf->datas[0].data && buf->datas[0].maxsize >= need)
    {
      const int idx = counter.fetch_add(1) % kIdxMod;
      g_sendNs[idx].store(nowNs(), std::memory_order_relaxed);
      fmt->fill(static_cast<uint8_t*>(buf->datas[0].data), w, h, stride, idx);
      buf->datas[0].chunk->offset = 0;
      buf->datas[0].chunk->stride = stride;
      buf->datas[0].chunk->size
          = uint32_t(frameBytesFor(fmt->spa, w, h, stride));
      queued.fetch_add(1);
    }
    pw_stream_queue_buffer(stream, b);
  }

  bool start(const PwFormat& f, const std::string& name, int width, int height,
             double rate, int stridePad)
  {
    fmt = &f;
    nodeName = name;
    w = width;
    h = height;
    fps = rate;
    stride = strideFor(f.spa, w) + stridePad;

    loop = pw_thread_loop_new("pwrt-src", nullptr);
    if(!loop)
      return false;
    pw_thread_loop_lock(loop);
    pw_thread_loop_start(loop);

    static const pw_stream_events events = [] {
      pw_stream_events e{};
      e.version = PW_VERSION_STREAM_EVENTS;
      e.process = &RawProducer::on_process_cb;
      e.param_changed = &RawProducer::on_param_changed_cb;
      return e;
    }();

    auto* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Camera", PW_KEY_NODE_NAME, nodeName.c_str(),
        // Publish as a camera-like device node: WirePlumber's standard
        // policy links consumers to Video/Source devices, not to
        // Stream/Output/Video peers.
        PW_KEY_MEDIA_CLASS, "Video/Source", nullptr);
    stream = pw_stream_new_simple(
        pw_thread_loop_get_loop(loop), nodeName.c_str(), props, &events, this);
    if(!stream)
    {
      pw_thread_loop_unlock(loop);
      return false;
    }

    uint8_t podBuf[1024];
    spa_pod_builder pb = SPA_POD_BUILDER_INIT(podBuf, sizeof(podBuf));
    spa_video_info_raw info{};
    info.format = spa_video_format(fmt->spa);
    info.size = SPA_RECTANGLE(uint32_t(w), uint32_t(h));
    info.framerate = SPA_FRACTION(uint32_t(fps * 1000), 1000);
    const spa_pod* params[1];
    params[0] = spa_format_video_raw_build(&pb, SPA_PARAM_EnumFormat, &info);

    const int rc = pw_stream_connect(
        stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
        pw_stream_flags(
            PW_STREAM_FLAG_DRIVER | PW_STREAM_FLAG_MAP_BUFFERS),
        params, 1);

    // Pace with a timer -> trigger_process (video-src.c pattern).
    timer = pw_loop_add_timer(
        pw_thread_loop_get_loop(loop), &RawProducer::on_timer_cb, this);
    timespec interval{};
    interval.tv_sec = 0;
    interval.tv_nsec = long(1e9 / fps);
    pw_loop_update_timer(
        pw_thread_loop_get_loop(loop), timer, &interval, &interval, false);

    pw_thread_loop_unlock(loop);
    return rc >= 0;
  }

  void stop()
  {
    if(!loop)
      return;
    // Stop the loop thread FIRST, then tear down without the lock:
    // pw_stream_destroy performs a blocking invoke on the stream's data
    // loop (== this loop), which deadlocks if the thread-loop lock is still
    // held while the loop thread runs.
    pw_thread_loop_stop(loop);
    if(timer)
      pw_loop_destroy_source(pw_thread_loop_get_loop(loop), timer);
    if(stream)
      pw_stream_destroy(stream);
    pw_thread_loop_destroy(loop);
    loop = nullptr;
    stream = nullptr;
    timer = nullptr;
  }
};

// ---------------------------------------------------------------------------
// Explicit link creation. WirePlumber's standard policy does not link
// Stream/Input/Video consumers to another app's video stream (only to
// Video/Source *devices*), and `target.object` alone doesn't create the
// link on this stack — so the harness wires the graph itself, pw-link
// style, by resolving the two nodes' ports via pw-dump and linking them.
// Runs on a helper thread; retries until the nodes/ports appear.
// ---------------------------------------------------------------------------
bool linkNodesOnce(const QString& outNode, const QString& inNode)
{
  QProcess dump;
  dump.start("pw-dump", {});
  if(!dump.waitForFinished(4000))
    return false;
  const auto doc = QJsonDocument::fromJson(dump.readAllStandardOutput());
  if(!doc.isArray())
    return false;

  std::map<int, QString> nodeNames; // node id -> node.name
  struct Port
  {
    int id, nodeId;
    bool out;
  };
  std::vector<Port> ports;
  for(const auto& v : doc.array())
  {
    const auto o = v.toObject();
    const auto type = o.value("type").toString();
    const auto info = o.value("info").toObject();
    const auto props = info.value("props").toObject();
    if(type == "PipeWire:Interface:Node")
      nodeNames[o.value("id").toInt()] = props.value("node.name").toString();
    else if(type == "PipeWire:Interface:Port")
      ports.push_back(
          {o.value("id").toInt(), props.value("node.id").toInt(),
           info.value("direction").toString() == "output"});
  }
  auto findPort = [&](const QString& nodeName, bool out) -> int {
    int best = -1;
    for(const auto& p : ports)
    {
      auto it = nodeNames.find(p.nodeId);
      if(it == nodeNames.end() || it->second != nodeName || p.out != out)
        continue;
      best = std::max(best, p.id); // latest instance wins
    }
    return best;
  };
  const int op = findPort(outNode, true), ip = findPort(inNode, false);
  if(op < 0 || ip < 0)
    return false;
  QProcess link;
  link.start("pw-link", {QString::number(op), QString::number(ip)});
  link.waitForFinished(3000);
  const auto err = QString::fromUtf8(link.readAllStandardError());
  // "File exists" = already linked: success.
  return link.exitCode() == 0 || err.contains("exist");
}

struct Linker
{
  std::thread thr;
  std::atomic<bool> stopFlag{false};
  std::atomic<bool> linked{false};

  void start(QString outNode, QString inNode)
  {
    thr = std::thread([this, outNode, inNode] {
      for(int i = 0; i < 50 && !stopFlag.load(); ++i)
      {
        if(linkNodesOnce(outNode, inNode))
        {
          linked = true;
          return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
    });
  }
  void stop()
  {
    stopFlag = true;
    if(thr.joinable())
      thr.join();
  }
};

// ---------------------------------------------------------------------------
// Receiver wrapper around score's InputStream.
// ---------------------------------------------------------------------------
struct Receiver
{
  std::shared_ptr<Video::ExternalInput> input;
  std::thread thr;
  std::atomic<bool> run{false};
  Metrics m;

  bool open(const QString& url)
  {
    input = Gfx::PipeWire::makePipewireCapture(url);
    return bool(input);
  }
  void start()
  {
    input->start();
    m.startNs = nowNs();
    run = true;
    thr = std::thread([this] {
      std::vector<uint8_t> rgba;
      while(run.load(std::memory_order_relaxed))
      {
        AVFrame* f = input->dequeue_frame();
        if(!f)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          continue;
        }
        m.onFrame(f, rgba);
        input->release_frame(f);
      }
    });
  }
  void stop()
  {
    run = false;
    if(thr.joinable())
      thr.join();
    if(input)
      input->stop();
  }
};

double runSeconds(double seconds)
{
  QEventLoop loop;
  QTimer::singleShot(qint64(seconds * 1000), &loop, [&] { loop.quit(); });
  loop.exec();
  return seconds;
}

Result finish(
    std::string cell, std::string transport, Metrics& m, double seconds,
    double psnrThreshold)
{
  Result r;
  r.cell = std::move(cell);
  r.transport = std::move(transport);
  r.recv = m.frames;
  r.gaps = m.gaps;
  r.repeats = m.repeats;
  r.badConvert = m.badConvert;
  r.fps = m.frames / seconds;
  r.minPsnr = m.psnrN ? m.psnrMin : 0;
  r.meanLatMs = m.latN ? m.latSumMs / m.latN : 0;
  r.drmPrime = m.drmPrime;
  if(m.frames == 0)
    r.status = "SKIP(no-frames)";
  else if(m.psnrN == 0)
    r.status = m.badConvert ? "FAIL(convert)" : "SKIP(no-verify)";
  else if(r.minPsnr < psnrThreshold)
    r.status = "FAIL(psnr)";
  else
    r.status = "PASS";
  return r;
}

// ---------------------------------------------------------------------------
// An offscreen sink: renders a graph into a texture and reads it back on
// demand. Reading back every frame would measure the readback instead.
// ---------------------------------------------------------------------------
class OffscreenSink final : public score::gfx::OutputNode
{
public:
  explicit OffscreenSink(QSize sz)
      : m_size{sz}
  {
    input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }
  ~OffscreenSink() override { }

  bool canRender() const override { return bool(m_renderState); }
  void startRendering() override { }
  void stopRendering() override { }
  void onRendererChange() override { }
  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override
  {
    m_renderer = r;
  }
  score::gfx::RenderList* renderer() const override { return m_renderer.lock().get(); }
  std::shared_ptr<score::gfx::RenderState> renderState() const override
  {
    return m_renderState;
  }
  Configuration configuration() const noexcept override
  {
    return {.manualRenderingRate = 1000. / 60.};
  }

  void createOutput(score::gfx::OutputConfiguration conf) override
  {
    m_renderState = score::gfx::createRenderState(conf.graphicsApi, m_size, nullptr);
    if(!m_renderState || !m_renderState->rhi)
    {
      m_renderState.reset();
      return;
    }
    m_renderState->outputSize = m_renderState->renderSize;
    auto rhi = m_renderState->rhi;
    m_renderState->renderFormat = QRhiTexture::RGBA8;
    m_texture = rhi->newTexture(
        QRhiTexture::RGBA8, m_renderState->renderSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    if(!m_texture->create())
    {
      m_renderState.reset();
      return;
    }
    m_renderTarget = rhi->newTextureRenderTarget({m_texture});
    m_renderState->renderPassDescriptor
        = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
    m_renderTarget->create();
    if(conf.onReady)
      conf.onReady();
  }

  void destroyOutput() override
  {
    if(!m_renderState)
      return;
    delete m_renderTarget;
    m_renderTarget = nullptr;
    delete m_renderState->renderPassDescriptor;
    m_renderState->renderPassDescriptor = nullptr;
    delete m_texture;
    m_texture = nullptr;
    m_renderState->destroy();
    m_renderState.reset();
  }

  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override
  {
    return new Gfx::BasicRenderer{
        score::gfx::TextureRenderTarget{
            .texture = m_texture,
            .renderPass = m_renderState->renderPassDescriptor,
            .renderTarget = m_renderTarget},
        *m_renderState, *this};
  }

  void render() override
  {
    auto renderer = m_renderer.lock();
    if(!renderer || !m_renderState)
      return;
    auto rhi = m_renderState->rhi;
    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return;
    renderer->render(*cb);
    // endOffscreenFrame waits for the GPU, so the caller's clock around
    // render() measures the frame and not the recording of it.
    rhi->endOffscreenFrame();
  }

  //! OpenGL puts the framebuffer origin at the bottom left, Vulkan at the
  //! top: a readback from the former arrives bottom-up and needs flipping.
  bool yUpInFramebuffer() const
  {
    return m_renderState && m_renderState->rhi
           && m_renderState->rhi->isYUpInFramebuffer();
  }

  //! One frame, read back. Separate from render() so a caller pays for the
  //! readback only when it wants pixels.
  QByteArray grab()
  {
    auto renderer = m_renderer.lock();
    if(!renderer || !m_renderState)
      return {};
    auto rhi = m_renderState->rhi;
    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return {};
    renderer->render(*cb);

    QRhiReadbackResult rb;
    auto* batch = rhi->nextResourceUpdateBatch();
    batch->readBackTexture(QRhiReadbackDescription{m_texture}, &rb);
    cb->resourceUpdate(batch);
    rhi->endOffscreenFrame();
    return rb.data;
  }

private:
  QSize m_size;
  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
};

// ---------------------------------------------------------------------------
// Cell A': score -> score, consumed on the GPU.
//
// Cell A below converts every frame on the CPU, which for a DMA-BUF frame
// means reading the shared buffer with the CPU. score exports it device-local,
// so that read is slow enough to dominate the frame -- all of it in the
// memcpy, mmap and munmap being negligible. Cell A's dmabuf fps and latency
// are therefore its own readback, and DRMPrimeDecoder, the consumer score
// actually uses, never runs there at all.
//
// This cell is that consumer: same producer, feeding a CameraNode rendered
// into an offscreen target, timed per frame. Pixels are checked once at the
// end through a single GPU readback.
// ---------------------------------------------------------------------------
Result runScoreToScoreGpu(
    score::gfx::GraphicsApi api, const char* apiName, const std::string& fmt,
    bool dmabuf, int w, int h, double fps, double seconds)
{
  const std::string cell = std::string("s2sgpu-") + apiName + "-" + fmt;
  const std::string transport = dmabuf ? "dmabuf" : "shm";

  Result r;
  r.cell = cell;
  r.transport = transport;

  Gfx::SharedOutputSettings s;
  const QString nodeName
      = QString("score-rtgpu-%1-%2").arg(apiName).arg(fmt.c_str());
  s.path = nodeName + "?format=" + QString::fromStdString(fmt)
           + (dmabuf ? "&dmabuf=on" : "");
  s.width = w;
  s.height = h;
  s.rate = fps;

  auto* src = new score::gfx::TexgenNode;
  src->function = &g_paint;
  score::gfx::OutputNode* out = Gfx::PipeWire::makePipewireOutput(s);

  auto prodGraph = std::make_unique<score::gfx::Graph>();
  prodGraph->addNode(src);
  prodGraph->addNode(out);
  prodGraph->addEdge(
      src->output[0], out->input[0], Process::CableType::ImmediateGlutton);
  prodGraph->createAllRenderLists(api);

  if(!out->canRender())
  {
    r.status = "SKIP(out-init)";
    prodGraph.reset();
    delete out;
    delete src;
    return r;
  }

  const QString url = QString("pipewire://%1?width=%2&height=%3&fps=%4&format=%5")
                          .arg(nodeName)
                          .arg(w)
                          .arg(h)
                          .arg(fps)
                          .arg(QString::fromStdString(fmt));
  auto input = Gfx::PipeWire::makePipewireCapture(url);
  if(!input)
  {
    r.status = "SKIP(in-open)";
    prodGraph.reset();
    delete out;
    delete src;
    return r;
  }

  // CameraNode pulls from the input and picks the decoder the frame's format
  // calls for -- DRMPrimeDecoder for the DMA-BUF frames this cell is about.
  auto* cam = new score::gfx::CameraNode{input};
  auto* sink = new OffscreenSink{QSize{w, h}};
  auto consGraph = std::make_unique<score::gfx::Graph>();
  consGraph->addNode(cam);
  consGraph->addNode(sink);
  consGraph->addEdge(
      cam->output[0], sink->input[0], Process::CableType::ImmediateGlutton);
  consGraph->createAllRenderLists(api);

  if(!sink->canRender())
  {
    r.status = "SKIP(in-init)";
    consGraph.reset();
    delete sink;
    prodGraph.reset();
    delete out;
    delete src;
    return r;
  }

  input->start();

  Linker lk;
  lk.start(nodeName, "PipewireRoundtrip");

  // Both ends on one timer, serially: that is how a score document with an
  // input and an output runs them.
  int consFrames = 0;
  int64_t consNs = 0;
  bool warm = true;
  QElapsedTimer warmup, countedFrom;
  warmup.start();

  QTimer render;
  render.setTimerType(Qt::PreciseTimer);
  QObject::connect(&render, &QTimer::timeout, [&] {
    out->render();
    // The first second is the link coming up and the decoder rebuilding on
    // the first DMA-BUF frame.
    if(warm && warmup.elapsed() > 1000)
    {
      warm = false;
      countedFrom.start();
    }
    const int64_t t0 = nowNs();
    // What the execution engine does for a camera node every tick: pull what
    // the device decoded and hand it to the renderer. Without this the node
    // has no current frame and the graph renders black, very fast.
    cam->process(score::gfx::Message{});
    sink->render();
    if(!warm)
    {
      consNs += nowNs() - t0;
      ++consFrames;
    }
  });
  render.start(int(1000.0 / fps));
  runSeconds(seconds);
  render.stop();
  lk.stop();

  const std::string wire
      = Gfx::PipeWire::pipewireInputNegotiatedTransport(*input).toStdString();
  const int queued = Gfx::PipeWire::pipewireOutputFramesQueued(*out);

  // Pixels, once: a consumer that negotiated the transport and drew nothing
  // would otherwise report an excellent number.
  std::vector<uint8_t> rgba;
  const QByteArray px = sink->grab();
  double psnr = 0;
  int idx = -1;
  if(px.size() >= qsizetype(w) * h * 4)
  {
    const auto* src = reinterpret_cast<const uint8_t*>(px.constData());
    rgba.resize(size_t(w) * h * 4);
    const size_t rowBytes = size_t(w) * 4;
    if(sink->yUpInFramebuffer())
      for(int y = 0; y < h; ++y)
        std::memcpy(
            rgba.data() + size_t(y) * rowBytes,
            src + size_t(h - 1 - y) * rowBytes, rowBytes);
    else
      std::memcpy(rgba.data(), src, rgba.size());
    idx = idxFromRgba(rgba.data(), w, h);
    if(!qgetenv("SCORE_PWRT_DUMP").isEmpty())
      QImage(rgba.data(), w, h, QImage::Format_RGBA8888)
          .save(QString::fromUtf8(qgetenv("SCORE_PWRT_DUMP")) + "-"
                + QString::fromStdString(cell) + "-"
                + QString::fromStdString(transport) + ".png");
    if(idx >= 0)
    {
      std::vector<uint8_t> ref(size_t(w) * h * 4);
      paint(ref.data(), w, h, idx);
      psnr = psnrGradient(rgba.data(), ref.data(), w, h);
    }
  }

  consGraph.reset();
  input->stop();
  delete sink;
  prodGraph.reset();
  delete out;
  delete src;

  r.wire = wire;
  r.sent = queued;
  r.recv = consFrames;
  // Over the counted window: the warm-up second is not in consFrames, and
  // dividing by the whole run would under-report by a third.
  const double countedSec = countedFrom.isValid()
                                ? double(countedFrom.elapsed()) / 1000.0
                                : 0.0;
  r.fps = countedSec > 0 ? consFrames / countedSec : 0;
  r.drmPrime = (wire == "dmabuf");
  r.minPsnr = psnr;
  // Not a latency: the mean cost of one consumer frame, which is what this
  // cell exists to report.
  r.meanLatMs = consFrames ? double(consNs) / 1e6 / consFrames : 0;

  if(consFrames == 0)
    r.status = lk.linked.load() ? "FAIL(no-frames)" : "SKIP(no-link)";
  else if(idx < 0)
    r.status = "FAIL(no-index)";
  else if(psnr < 24.0)
    r.status = "FAIL(psnr)";
  else if(dmabuf && wire != "dmabuf")
    r.status = "PASS(fallback)";
  else
    r.status = "PASS";
  return r;
}

// ---------------------------------------------------------------------------
// Cell A: score -> score round-trip, verified on the CPU.
//
// The pixel-correctness cell: swscale converts every frame, which is what lets
// it check every format score can emit. Read its dmabuf rows for `status` and
// `minPSNR` only -- their fps and latency are the harness reading a
// device-local buffer with the CPU, not score. s2sgpu above measures that.
// ---------------------------------------------------------------------------
Result runScoreToScore(
    score::gfx::GraphicsApi api, const char* apiName, const std::string& fmt,
    bool dmabuf, int w, int h, double fps, double seconds)
{
  const std::string cell = std::string("s2s-") + apiName + "-" + fmt;
  const std::string transport = dmabuf ? "dmabuf" : "shm";

  Gfx::SharedOutputSettings s;
  const QString nodeName
      = QString("score-rt-%1-%2").arg(apiName).arg(fmt.c_str());
  s.path = nodeName + "?format=" + QString::fromStdString(fmt)
           + (dmabuf ? "&dmabuf=on" : "");
  s.width = w;
  s.height = h;
  s.rate = fps;

  auto* src = new score::gfx::TexgenNode;
  src->function = &g_paint;
  score::gfx::OutputNode* out = Gfx::PipeWire::makePipewireOutput(s);

  auto graph = std::make_unique<score::gfx::Graph>();
  graph->addNode(src);
  graph->addNode(out);
  graph->addEdge(src->output[0], out->input[0], Process::CableType::ImmediateGlutton);
  graph->createAllRenderLists(api);

  Result r;
  r.cell = cell;
  r.transport = transport;
  if(!out->canRender())
  {
    r.status = "SKIP(out-init)";
    graph.reset();
    delete out;
    delete src;
    return r;
  }

  // Receiver: connect to the producer node by name.
  Receiver rcv;
  const QString url = QString("pipewire://%1?width=%2&height=%3&fps=%4&format=%5")
                          .arg(nodeName)
                          .arg(w)
                          .arg(h)
                          .arg(fps)
                          .arg(QString::fromStdString(fmt));
  if(!rcv.open(url))
  {
    r.status = "SKIP(in-open)";
    graph.reset();
    delete out;
    delete src;
    return r;
  }
  rcv.start();

  Linker lk;
  lk.start(nodeName, "PipewireRoundtrip");

  QTimer render;
  render.setTimerType(Qt::PreciseTimer);
  QObject::connect(&render, &QTimer::timeout, [&] { out->render(); });
  render.start(int(1000.0 / fps));
  runSeconds(seconds);
  render.stop();
  lk.stop();
  // Transport truth must be sampled BEFORE stop(): stop() resets the
  // negotiation latch.
  const std::string wire
      = Gfx::PipeWire::pipewireInputNegotiatedTransport(*rcv.input)
            .toStdString();
  // Sampled before teardown, for the same reason as the transport: what the
  // producer managed to publish is the difference between an environment that
  // could not run the cell and a cell that ran and lost every frame.
  const int queued = Gfx::PipeWire::pipewireOutputFramesQueued(*out);
  rcv.stop();

  r = finish(cell, transport, rcv.m, seconds, 24.0);
  r.sent = queued;
  // Same demotion ladder as runPwToScore. The last rung matters: a cell whose
  // link was made and whose producer published, but whose consumer received
  // nothing, is a failure, not a skip -- only FAIL sets the exit code, so a
  // skip there would let the whole matrix exit 0 while testing nothing.
  if(r.status == "SKIP(no-frames)")
  {
    if(!lk.linked.load())
      r.status = "SKIP(no-link)";
    else if(queued > 0)
      r.status = "FAIL(no-frames)";
  }
  // Transport truth: what the producer engaged and what the consumer
  // negotiated — a dmabuf-requested cell that ran on fallback must say so.
  const bool prodDma = Gfx::PipeWire::pipewireOutputDmabufEngaged(*out);
  r.wire = wire;
  if(dmabuf && !prodDma)
  {
    r.transport = "dma!fb"; // requested dmabuf, producer fell back
    if(r.status == "PASS")
      r.status = "PASS(fallback)";
  }
  graph.reset();
  delete out;
  delete src;
  return r;
}

// ---------------------------------------------------------------------------
// Cell C: score -> GStreamer, and GStreamer -> score.
//
// The two directions a user actually reaches for: OBS, a GStreamer pipeline,
// anything that is not score. They go through the session manager rather than
// through a link this harness makes by hand, so they are the only cells that
// notice whether score's output is reachable from outside at all.
//
// Both pass. s2gst was the reproduction of what OBS saw and took two fixes to
// get there: media.class had to say Video/Source before anything would link at
// all, and then the link was made, the format negotiated, and not one usable
// buffer arrived -- score announced no SPA_PARAM_Buffers, so the server handed
// out buffers whose maxsize was zero and score dutifully queued hundreds of
// empty ones. Run either by name:
//   PipewireRoundtrip --only s2gst
// ---------------------------------------------------------------------------
bool haveGstLaunch()
{
  QProcess p;
  p.start("gst-launch-1.0", {"--version"});
  return p.waitForFinished(4000) && p.exitCode() == 0;
}

//! object.serial of the node named \p name, or -1.
int64_t nodeSerial(const QString& name)
{
  QProcess dump;
  dump.start("pw-dump", {});
  if(!dump.waitForFinished(4000))
    return -1;
  const auto doc = QJsonDocument::fromJson(dump.readAllStandardOutput());
  if(!doc.isArray())
    return -1;
  int64_t best = -1;
  for(const auto& v : doc.array())
  {
    const auto o = v.toObject();
    if(o.value("type").toString() != "PipeWire:Interface:Node")
      continue;
    const auto props
        = o.value("info").toObject().value("props").toObject();
    if(props.value("node.name").toString() != name)
      continue;
    best = std::max<int64_t>(best, props.value("object.serial").toInteger(-1));
  }
  return best;
}

//! Not a flat image: the test signal is a gradient, so a frame that came
//! through carries a spread of values. Guards against a consumer that gets
//! buffers of zeroes.
bool looksLikeTestSignal(const QImage& img)
{
  if(img.isNull() || img.width() < 8 || img.height() < 8)
    return false;
  int lo = 255, hi = 0;
  for(int y = img.height() / 2; y < img.height(); y += 4)
    for(int x = 0; x < img.width(); x += 4)
    {
      const int v = qGray(img.pixel(x, y));
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
  return (hi - lo) > 40;
}

Result runScoreToGstreamer(
    score::gfx::GraphicsApi api, const char* apiName, int w, int h, double fps,
    double seconds, bool dmabuf)
{
  const std::string cell = std::string("s2gst-") + apiName;
  Result r;
  r.cell = cell;
  r.transport = dmabuf ? "dmabuf" : "shm";

  if(!haveGstLaunch())
  {
    r.status = "SKIP(no-gst)";
    return r;
  }

  Gfx::SharedOutputSettings s;
  const QString nodeName = QString("score-rt-gst-%1").arg(apiName);
  s.path = nodeName + "?format=rgba" + (dmabuf ? "&dmabuf=on" : "");
  s.width = w;
  s.height = h;
  s.rate = fps;

  auto* src = new score::gfx::TexgenNode;
  src->function = &g_paint;
  score::gfx::OutputNode* out = Gfx::PipeWire::makePipewireOutput(s);

  auto graph = std::make_unique<score::gfx::Graph>();
  graph->addNode(src);
  graph->addNode(out);
  graph->addEdge(src->output[0], out->input[0], Process::CableType::ImmediateGlutton);
  graph->createAllRenderLists(api);

  if(!out->canRender())
  {
    r.status = "SKIP(out-init)";
    graph.reset();
    delete out;
    delete src;
    return r;
  }

  QTimer render;
  render.setTimerType(Qt::PreciseTimer);
  QObject::connect(&render, &QTimer::timeout, [&] { out->render(); });
  render.start(int(1000.0 / fps));

  // Let the node appear and the stream settle before asking for it by name.
  runSeconds(std::min(2.0, seconds));

  const int64_t serial = nodeSerial(nodeName);
  if(serial < 0)
  {
    render.stop();
    r.status = "SKIP(no-node)";
    graph.reset();
    delete out;
    delete src;
    return r;
  }

  QTemporaryDir dir;
  QProcess gst;
  gst.setProcessChannelMode(QProcess::MergedChannels);
  gst.start(
      "gst-launch-1.0",
      {"pipewiresrc", QString("target-object=%1").arg(serial), "num-buffers=8",
       "!", "videoconvert", "!", "video/x-raw,format=RGB", "!", "pngenc", "!",
       "multifilesink", QString("location=%1/f_%2.png").arg(dir.path(), "%d")});

  // WirePlumber's policy is what is under test, but if it declines to make
  // the link the harness must still be able to say so rather than report a
  // dead cell: wire them up by hand after a grace period.
  Linker lk;
  lk.start(nodeName, "gst-launch-1.0");

  // The pipeline only runs while score keeps rendering, so pump the event loop
  // rather than blocking on the process.
  QElapsedTimer t;
  t.start();
  while(gst.state() != QProcess::NotRunning && t.elapsed() < seconds * 1000 + 8000)
  {
    QApplication::processEvents(QEventLoop::AllEvents, 10);
    gst.waitForFinished(20);
  }
  if(gst.state() != QProcess::NotRunning)
    gst.kill();
  gst.waitForFinished(2000);

  lk.stop();
  render.stop();
  const int queued = Gfx::PipeWire::pipewireOutputFramesQueued(*out);
  r.sent = queued;

  const auto files = QDir{dir.path()}.entryList({"f_*.png"}, QDir::Files);
  if(files.isEmpty())
    std::printf(
        "  gst-launch said: %s\n", gst.readAll().constData());
  r.recv = files.size();
  r.fps = r.recv / seconds;

  if(files.isEmpty())
  {
    // The producer published and the consumer got nothing: that is the defect,
    // not a missing environment.
    r.status = queued > 0 ? "FAIL(no-frames)" : "SKIP(no-frames)";
  }
  else
  {
    QImage img{dir.filePath(files.last())};
    if(img.width() != w || img.height() != h)
      r.status = "FAIL(geometry)";
    else if(!looksLikeTestSignal(img))
      r.status = "FAIL(blank)";
    else
      r.status = "PASS";
  }

  graph.reset();
  delete out;
  delete src;
  return r;
}

Result runGstreamerToScore(int w, int h, double fps, double seconds, bool dmabuf)
{
  const std::string cell = "gst2s";
  Result r;
  r.cell = cell;
  r.transport = dmabuf ? "dmabuf" : "shm";

  if(!haveGstLaunch())
  {
    r.status = "SKIP(no-gst)";
    return r;
  }

  const QString nodeName = QStringLiteral("pwrt-gst-src");
  QProcess gst;
  gst.start(
      "gst-launch-1.0",
      {"videotestsrc", "is-live=true", "!",
       QString("video/x-raw,format=RGBA,width=%1,height=%2,framerate=%3/1")
           .arg(w)
           .arg(h)
           .arg(int(fps)),
       "!", "pipewiresink", "mode=provide",
       QString("stream-properties=p,node.name=%1").arg(nodeName)});
  if(!gst.waitForStarted(4000))
  {
    r.status = "SKIP(no-gst)";
    return r;
  }

  Receiver rcv;
  // dmabuf=off is what the panel writes; it makes the consumer offer no
  // modifiers, so the producer can only answer with shared memory.
  const QString url = QString("pipewire://%1?width=%2&height=%3&fps=%4&format=rgba%5")
                          .arg(nodeName)
                          .arg(w)
                          .arg(h)
                          .arg(fps)
                          .arg(dmabuf ? "" : "&dmabuf=off");
  if(!rcv.open(url))
  {
    gst.kill();
    gst.waitForFinished(2000);
    r.status = "SKIP(in-open)";
    return r;
  }
  rcv.start();

  Linker lk;
  lk.start(nodeName, "PipewireRoundtrip");

  runSeconds(seconds);
  lk.stop();
  rcv.stop();
  gst.kill();
  gst.waitForFinished(2000);

  r.recv = rcv.m.frames;
  r.fps = r.recv / seconds;
  // videotestsrc paints its own pattern, so the index/PSNR verification does
  // not apply: what is under test is that frames cross the boundary at all.
  if(r.recv == 0)
    r.status = lk.linked.load() ? "FAIL(no-frames)" : "SKIP(no-link)";
  else
    r.status = "PASS";
  return r;
}

// ---------------------------------------------------------------------------
// Cell B: raw pw producer -> score InputStream.
// ---------------------------------------------------------------------------
Result runPwToScore(
    const PwFormat& f, int w, int h, double fps, double seconds, int stridePad,
    bool forceShm)
{
  const std::string cell = std::string("pw2s-") + f.name
                           + (stridePad ? "-padded" : "");
  if(forceShm)
    qputenv("SCORE_PIPEWIRE_FORCE_SHM", "1");
  else
    qunsetenv("SCORE_PIPEWIRE_FORCE_SHM");

  RawProducer prod;
  const std::string nodeName = std::string("pwrt-src-") + f.name;
  Result r;
  r.cell = cell;
  r.transport = "shm";
  if(!prod.start(f, nodeName, w, h, fps, stridePad))
  {
    r.status = "SKIP(prod-init)";
    return r;
  }

  Receiver rcv;
  const QString url
      = QString("pipewire://%1?width=%2&height=%3&fps=%4&format=%5")
            .arg(nodeName.c_str())
            .arg(w)
            .arg(h)
            .arg(fps)
            .arg(f.urlFormat);
  if(!rcv.open(url))
  {
    prod.stop();
    r.status = "SKIP(in-open)";
    return r;
  }
  rcv.start();
  Linker lk;
  lk.start(QString::fromStdString(nodeName), "PipewireRoundtrip");
  runSeconds(seconds);
  lk.stop();
  const std::string wire
      = rcv.input ? Gfx::PipeWire::pipewireInputNegotiatedTransport(*rcv.input)
                        .toStdString()
                  : std::string{};
  rcv.stop();
  prod.stop();
  qunsetenv("SCORE_PIPEWIRE_FORCE_SHM");

  // YUV 4:2:0 double-conversion is lossier than SDI: accept >= 20 dB.
  const double thr = 20.0;
  r = finish(cell, r.transport, rcv.m, seconds, thr);
  r.wire = wire;
  r.sent = prod.queued.load();
  if(r.status == "SKIP(no-frames)")
  {
    if(!lk.linked.load())
      r.status = "SKIP(no-link)";
    else if(prod.queued.load() > 0)
      r.status = "FAIL(no-frames)"; // linked + producer ran; consumer got 0
  }
  return r;
}

void printMatrix(const std::vector<Result>& rows)
{
  std::printf(
      "\n%-28s %-7s %-7s %6s %6s %6s %5s %5s %5s %8s %8s %-5s %s\n", "cell",
      "trans", "wire", "sent", "recv", "fps", "lost", "rep", "badcv", "lat(ms)",
      "minPSNR", "drm", "status");
  std::printf("%s\n", std::string(125, '-').c_str());
  for(const auto& r : rows)
    std::printf(
        "%-28s %-7s %-7s %6d %6d %6.1f %5d %5d %5d %8.2f %8.2f %-5s %s\n",
        r.cell.c_str(), r.transport.c_str(),
        r.wire.empty() ? "-" : r.wire.c_str(), r.sent, r.recv, r.fps, r.gaps,
        r.repeats, r.badConvert, r.meanLatMs, r.minPsnr,
        r.drmPrime ? "yes" : "no", r.status.c_str());

  // The s2sgpu rows use two of these columns for something else.
  bool anyGpu = false;
  for(const auto& r : rows)
    if(r.cell.rfind("s2sgpu-", 0) == 0)
      anyGpu = true;
  if(anyGpu)
    std::printf(
        "\ns2sgpu rows: lat(ms) is the mean cost of one consumer frame, not a "
        "latency, and\nrecv counts consumer frames after a one-second warm-up. "
        "The other rows verify on\nthe CPU, so their dmabuf timings are the "
        "harness reading device-local memory.\n");
}

} // namespace

int main(int argc, char** argv)
{
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  qputenv("SCORE_AUDIO_BACKEND", "dummy");
  // Match score's production startup (src/app/main.cpp): EGL is the only
  // way to get zero-copy dma-buf import on X11; Qt defaults to GLX.
#if defined(__linux__)
  if(!qEnvironmentVariableIsSet("QT_XCB_GL_INTEGRATION"))
    qputenv("QT_XCB_GL_INTEGRATION", "xcb_egl");
#endif

  // Static builds never run Application::loadResources(); register the
  // score_lib_base resources by hand or startup aborts on the missing skin
  // (same workaround as AJARoundtrip).
#if defined(SCORE_STATIC_PLUGINS)
  Q_INIT_RESOURCE(score);
  Q_INIT_RESOURCE(fonts);
#endif

  QApplication app(argc, argv);
  pw_init(nullptr, nullptr);

  QTimer dialogKiller;
  QObject::connect(&dialogKiller, &QTimer::timeout, [] {
    if(auto* w = QApplication::activeModalWidget())
      w->close();
  });
  dialogKiller.start(100);

  int rc = 0;
  QMetaObject::invokeMethod(
      &app,
      [&] {
        QCommandLineParser p;
        p.addHelpOption();
        QCommandLineOption secs("seconds", "Seconds per cell", "s", "3");
        QCommandLineOption only("only", "Comma list of cell substrings", "l");
        QCommandLineOption apiOpt("api", "opengl | vulkan | all", "api", "all");
        QCommandLineOption sizeOpt("size", "WxH", "wxh", "1280x720");
        p.addOptions({secs, only, apiOpt, sizeOpt});
        p.process(*qApp);
        const double seconds = p.value(secs).toDouble();
        const QStringList filters
            = p.isSet(only) ? p.value(only).split(',') : QStringList{};
        auto want = [&](const std::string& cell) {
          if(filters.isEmpty())
            return true;
          for(const auto& f : filters)
            if(cell.find(f.toStdString()) != std::string::npos)
              return true;
          return false;
        };
        const auto sz = p.value(sizeOpt).split('x');
        const int W = sz.value(0).toInt(), H = sz.value(1).toInt();
        const double FPS = 30.;

        std::vector<Result> rows;

        // --- A: score -> score ---
        struct ApiCfg
        {
          score::gfx::GraphicsApi api;
          const char* name;
        };
        std::vector<ApiCfg> apis;
        const QString apiSel = p.value(apiOpt);
        if(apiSel == "all" || apiSel == "opengl")
          apis.push_back({score::gfx::GraphicsApi::OpenGL, "gl"});
        if(apiSel == "all" || apiSel == "vulkan")
          apis.push_back({score::gfx::GraphicsApi::Vulkan, "vk"});

        const std::vector<std::string> outFmts
            = {"rgba", "bgra", "rgb10a2", "bgr10a2", "rgba16f"};
        for(const auto& a : apis)
          for(const auto& f : outFmts)
          {
            const std::string cell = std::string("s2s-") + a.name + "-" + f;
            if(!want(cell))
              continue;
            std::printf("[ %-26s shm ] running...\n", cell.c_str());
            std::fflush(stdout);
            rows.push_back(
                runScoreToScore(a.api, a.name, f, false, W, H, FPS, seconds));
          }
        // dmabuf output: EGL/GBM path on GL, exportable-VkImage path on
        // Vulkan (the latter needs Qt >= 6.6; on older Qt the producer
        // falls back and the cell reports PASS(fallback)).
        for(const auto& a : apis)
        {
          for(const std::string f : {"rgba", "bgra"})
          {
            const std::string cell = std::string("s2s-") + a.name + "-" + f;
            if(!want(cell + "-dmabuf"))
              continue;
            std::printf("[ %-26s dmabuf ] running...\n", cell.c_str());
            std::fflush(stdout);
            rows.push_back(
                runScoreToScore(a.api, a.name, f, true, W, H, FPS, seconds));
          }
        }

        // --- A': score -> score, consumed on the GPU ---
        for(const auto& a : apis)
        {
          for(const std::string f : {"rgba", "bgra"})
          {
            const std::string cell = std::string("s2sgpu-") + a.name + "-" + f;
            for(const bool dma : {false, true})
            {
              if(!want(cell + (dma ? "-dmabuf" : "-shm")))
                continue;
              std::printf(
                  "[ %-26s %s ] running...\n", cell.c_str(),
                  dma ? "dmabuf" : "shm");
              std::fflush(stdout);
              rows.push_back(
                  runScoreToScoreGpu(a.api, a.name, f, dma, W, H, FPS, seconds));
            }
          }
        }

        // --- B: raw pw producer -> score input, every input format ---
        for(const auto& f : pwFormats())
        {
          const std::string cell = std::string("pw2s-") + f.name;
          if(want(cell))
          {
            std::printf("[ %-26s shm ] running...\n", cell.c_str());
            std::fflush(stdout);
            rows.push_back(runPwToScore(f, W, H, FPS, seconds, 0, false));
          }
        }
        // Padded-stride robustness (packed RGBA + planar I420).
        for(const auto& f : pwFormats())
        {
          if(strcmp(f.name, "rgba") && strcmp(f.name, "yuv420p"))
            continue;
          const std::string cell = std::string("pw2s-") + f.name + "-padded";
          if(!want(cell))
            continue;
          std::printf("[ %-26s shm ] running...\n", cell.c_str());
          std::fflush(stdout);
          rows.push_back(runPwToScore(f, W, H, FPS, seconds, 64, false));
        }

        // --- C: score <-> gstreamer, through the session manager ---
        for(const bool dmabuf : {false, true})
        {
          for(const auto& a : apis)
          {
            const std::string cell = std::string("s2gst-") + a.name;
            if(!want(cell))
              continue;
            std::printf(
                "[ %-26s %s ] running...\n", cell.c_str(), dmabuf ? "dmabuf" : "shm");
            std::fflush(stdout);
            rows.push_back(
                runScoreToGstreamer(a.api, a.name, W, H, FPS, seconds, dmabuf));
          }
          if(want("gst2s"))
          {
            std::printf("[ %-26s %s ] running...\n", "gst2s", dmabuf ? "dmabuf" : "shm");
            std::fflush(stdout);
            rows.push_back(runGstreamerToScore(W, H, FPS, seconds, dmabuf));
          }
        }

        printMatrix(rows);
        const bool anyFail
            = std::any_of(rows.begin(), rows.end(), [](const Result& r) {
                return r.status.rfind("FAIL", 0) == 0;
              });
        rc = anyFail ? 1 : 0;
        std::fflush(stdout);
        std::_Exit(rc);
      },
      Qt::QueuedConnection);

  return app.exec();
}
