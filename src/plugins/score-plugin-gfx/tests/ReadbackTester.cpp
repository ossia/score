// Offscreen self-test + micro-benchmark for RhiTextureReadback: uploads a
// known RGBA pattern, reads it back into caller-owned host memory through the
// native path, and byte-compares against QRhi::readBackTexture. Then times
// both paths at 3840x2160. Backend via SCORE_TEST_API=vulkan|opengl.

#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/RhiTextureReadback.hpp>
#include <Gfx/Graph/interop/VkHostImportUpload.hpp>

#include <core/application/MinimalApplication.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <QtGui/private/qrhi_p.h>

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace score::gfx;

namespace
{
int g_fail = 0;
void check(bool ok, const char* what)
{
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok)
    ++g_fail;
}

std::vector<uint8_t> makePattern(int w, int h)
{
  std::vector<uint8_t> px(size_t(w) * h * 4);
  for(int y = 0; y < h; ++y)
    for(int x = 0; x < w; ++x)
    {
      uint8_t* p = px.data() + (size_t(y) * w + x) * 4;
      p[0] = uint8_t(x);
      p[1] = uint8_t(y);
      p[2] = uint8_t(x ^ y);
      p[3] = 255;
    }
  return px;
}

QRhiTexture* makeUploadedTexture(QRhi& rhi, int w, int h, const std::vector<uint8_t>& px)
{
  auto* tex
      = rhi.newTexture(QRhiTexture::RGBA8, QSize(w, h), 1, QRhiTexture::UsedAsTransferSource);
  tex->create();
  QRhiCommandBuffer* cb{};
  rhi.beginOffscreenFrame(&cb);
  auto* b = rhi.nextResourceUpdateBatch();
  QRhiTextureSubresourceUploadDescription sub{
      QByteArray(reinterpret_cast<const char*>(px.data()), int(px.size()))};
  b->uploadTexture(tex, QRhiTextureUploadDescription{{0, 0, sub}});
  cb->resourceUpdate(b);
  rhi.endOffscreenFrame();
  return tex;
}

void testCorrectness(QRhi& rhi)
{
  const int W = 256, H = 128;
  const auto px = makePattern(W, H);
  auto* tex = makeUploadedTexture(rhi, W, H, px);

  const std::size_t align = readbackHostMemoryAlignment(rhi);
  std::printf(
      "canReadbackToHostMemory=%d alignment=%zu\n", int(canReadbackToHostMemory(rhi)),
      align);
  check(canReadbackToHostMemory(rhi), "canReadbackToHostMemory");
  if(!canReadbackToHostMemory(rhi))
    return;

  const std::size_t bytes = px.size();
  void* dst = interop::alignedSlotAlloc(bytes, align ? align : 4096);
  std::memset(dst, 0xAB, bytes);

  auto* target = createReadbackTarget(rhi, dst, bytes);
  check(target != nullptr, "createReadbackTarget");
  if(!target)
  {
    interop::alignedSlotFree(dst);
    delete tex;
    return;
  }

  QRhiCommandBuffer* cb{};
  rhi.beginOffscreenFrame(&cb);
  const bool recorded = readbackTextureToHost(rhi, *cb, *tex, *target);
  rhi.endOffscreenFrame();
  check(recorded, "readbackTextureToHost recorded");
  check(finishReadbackToHost(rhi, *target), "finishReadbackToHost");

  const bool same = recorded && std::memcmp(dst, px.data(), bytes) == 0;
  check(same, "bytes identical to uploaded pattern");
  if(!same && recorded)
  {
    const auto* d = static_cast<const uint8_t*>(dst);
    std::printf(
        "  first bytes: got %d,%d,%d,%d want %d,%d,%d,%d\n", d[0], d[1], d[2], d[3],
        px[0], px[1], px[2], px[3]);
  }

  // Second frame must work with the same target (no per-frame setup).
  std::memset(dst, 0xCD, bytes);
  rhi.beginOffscreenFrame(&cb);
  const bool recorded2 = readbackTextureToHost(rhi, *cb, *tex, *target);
  rhi.endOffscreenFrame();
  finishReadbackToHost(rhi, *target);
  check(recorded2 && std::memcmp(dst, px.data(), bytes) == 0, "second frame identical");

  destroyReadbackTarget(target);
  interop::alignedSlotFree(dst);
  delete tex;
}

void benchmark(QRhi& rhi)
{
  const int W = 3840, H = 2160, N = 60;
  const auto px = makePattern(W, H);
  auto* tex = makeUploadedTexture(rhi, W, H, px);
  const std::size_t bytes = px.size();
  const std::size_t align = readbackHostMemoryAlignment(rhi);
  void* dst = interop::alignedSlotAlloc(bytes, align ? align : 4096);

  auto* target = createReadbackTarget(rhi, dst, bytes);
  if(!target)
  {
    std::printf("benchmark skipped: no readback target\n");
    interop::alignedSlotFree(dst);
    delete tex;
    return;
  }

  QRhiCommandBuffer* cb{};
  QElapsedTimer t;

  // Warmup + host-memory path
  for(int i = 0; i < 5; ++i)
  {
    rhi.beginOffscreenFrame(&cb);
    readbackTextureToHost(rhi, *cb, *tex, *target);
    rhi.endOffscreenFrame();
    finishReadbackToHost(rhi, *target);
  }
  t.start();
  for(int i = 0; i < N; ++i)
  {
    rhi.beginOffscreenFrame(&cb);
    readbackTextureToHost(rhi, *cb, *tex, *target);
    rhi.endOffscreenFrame();
    finishReadbackToHost(rhi, *target);
  }
  const double hostMs = t.nsecsElapsed() / 1e6 / N;

  // Baseline: QRhi::readBackTexture + memcpy into dst
  QRhiReadbackResult rb;
  for(int i = 0; i < 5; ++i)
  {
    rhi.beginOffscreenFrame(&cb);
    auto* b = rhi.nextResourceUpdateBatch();
    b->readBackTexture(QRhiReadbackDescription{tex}, &rb);
    cb->resourceUpdate(b);
    rhi.endOffscreenFrame();
    std::memcpy(dst, rb.data.constData(), bytes);
  }
  t.restart();
  for(int i = 0; i < N; ++i)
  {
    rhi.beginOffscreenFrame(&cb);
    auto* b = rhi.nextResourceUpdateBatch();
    b->readBackTexture(QRhiReadbackDescription{tex}, &rb);
    cb->resourceUpdate(b);
    rhi.endOffscreenFrame();
    std::memcpy(dst, rb.data.constData(), bytes);
  }
  const double qrhiMs = t.nsecsElapsed() / 1e6 / N;

  std::printf(
      "benchmark 3840x2160 RGBA8, %d frames:\n"
      "  host-memory readback : %.3f ms/frame\n"
      "  QRhi readBackTexture : %.3f ms/frame (+memcpy into dst)\n",
      N, hostMs, qrhiMs);

  destroyReadbackTarget(target);
  interop::alignedSlotFree(dst);
  delete tex;
}

void runTests()
{
  const QByteArray apiEnv = qgetenv("SCORE_TEST_API").toLower();
  const GraphicsApi api = (apiEnv == "vulkan" || apiEnv == "vk")
                              ? GraphicsApi::Vulkan
                              : GraphicsApi::OpenGL;
  auto state = createRenderState(api, QSize(256, 128), nullptr);
  if(!state || !state->rhi)
  {
    std::printf("ERROR: no QRhi (need a GL-capable display)\n");
    ++g_fail;
    return;
  }
  auto& rhi = *state->rhi;
  std::printf("backend=%s\n", rhi.backendName());

  testCorrectness(rhi);
  if(!g_fail)
    benchmark(rhi);

  std::printf("\n%s (%d failures)\n", g_fail ? "FAILED" : "ALL PASS", g_fail);
}
} // namespace

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define SCORE_TEST_HAS_LSAN 1
#endif
#elif defined(__SANITIZE_ADDRESS__)
#define SCORE_TEST_HAS_LSAN 1
#endif
#if defined(SCORE_TEST_HAS_LSAN)
#include <sanitizer/lsan_interface.h>
#endif

int main(int argc, char** argv)
{
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  qputenv("SCORE_AUDIO_BACKEND", "dummy");

  score::MinimalGUIApplication app(argc, argv);

  QTimer dialogKiller;
  QObject::connect(&dialogKiller, &QTimer::timeout, [] {
    if(auto* w = QApplication::activeModalWidget())
      w->close();
  });
  dialogKiller.start(100);

  QMetaObject::invokeMethod(
      &app,
      [] {
        runTests();
        std::fflush(stdout);
        #if defined(SCORE_TEST_HAS_LSAN)
        // _Exit skips LSan's atexit hook; run the leak check explicitly so
        // sanitizer builds still report leaks (dies non-zero on findings).
        __lsan_do_leak_check();
#endif
        std::_Exit(g_fail ? 1 : 0);
      },
      Qt::QueuedConnection);
  return app.exec();
}
