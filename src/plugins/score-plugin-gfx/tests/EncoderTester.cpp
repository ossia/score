// Offscreen self-test for the GPU video encoders. Runs each encoder on a
// known RGBA gray gradient and checks the readback planes for:
//   - correct 10-bit bit packing (low 10 bits for yuv422p10le, high 10 bits
//     i.e. multiple of 64 for p010),
//   - neutral chroma for a gray input (U == V == 512 in 10-bit),
//   - monotonically increasing luma across the gradient.
// No AJA / libav / gstreamer needed; just a QRhi offscreen context.

#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/encoders/P010.hpp>
#include <Gfx/Graph/encoders/YUV422P10.hpp>

#include <core/application/MinimalApplication.hpp>

#include <QApplication>
#include <QTimer>

#include <clocale>
#include <cstdio>
#include <cstdlib>
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

// uint16 sample at (x,y), component `comp`. The QRhi GL backend reads single-
// and dual-channel 16-bit textures back PADDED (R16 -> 4 bytes/pixel, RG16 ->
// 8), so derive the real per-pixel uint16 stride from the readback size.
uint16_t sample(const QRhiReadbackResult& rb, int x, int y, int comp)
{
  const int w = rb.pixelSize.width(), h = rb.pixelSize.height();
  const int u16pp = int(rb.data.size() / (size_t(w) * h) / 2);
  const auto* p = reinterpret_cast<const uint16_t*>(rb.data.constData());
  return p[((size_t(y) * w + x) * u16pp) + comp];
}

// Per-pixel uint16 stride of a readback plane (incl. backend padding).
int u16stride(const QRhiReadbackResult& rb)
{
  const int w = rb.pixelSize.width(), h = rb.pixelSize.height();
  return int(rb.data.size() / (size_t(w) * h) / 2);
}

// Upload the input pixels and run the encoder in the SAME offscreen frame
// (the upload must be submitted before the encoder's passes sample it).
void uploadAndExec(
    QRhi& rhi, GPUVideoEncoder& enc, QRhiTexture* input,
    const std::vector<uint8_t>& px)
{
  QRhiCommandBuffer* cb{};
  rhi.beginOffscreenFrame(&cb);
  auto* batch = rhi.nextResourceUpdateBatch();
  QRhiTextureSubresourceUploadDescription sub{
      QByteArray(reinterpret_cast<const char*>(px.data()), int(px.size()))};
  batch->uploadTexture(input, QRhiTextureUploadDescription{{0, 0, sub}});
  cb->resourceUpdate(batch);
  enc.exec(rhi, *cb);
  rhi.endOffscreenFrame();
}

void testYUV422P10(
    QRhi& rhi, const RenderState& state, QRhiTexture* input,
    const std::vector<uint8_t>& px, int w, int h)
{
  std::printf("YUV422P10Encoder (planar 4:2:2, low 10 bits):\n");
  YUV422P10Encoder enc;
  enc.init(rhi, state, input, w, h);
  uploadAndExec(rhi, enc, input, px);

  const auto& Y = enc.readback(0);
  const auto& U = enc.readback(1);
  const auto& V = enc.readback(2);
  check(!Y.data.isEmpty() && !U.data.isEmpty() && !V.data.isEmpty(), "readbacks present");

  // Bit packing: every Y value (component 0) must fit in 10 bits.
  bool low10 = true;
  for(int yy = 0; yy < Y.pixelSize.height() && low10; ++yy)
    for(int xx = 0; xx < Y.pixelSize.width(); ++xx)
      if(sample(Y, xx, yy, 0) > 1023) { low10 = false; break; }
  check(low10, "Y values fit in low 10 bits (<=1023)");

  // Neutral chroma for gray: U == V == 512.
  const int u = sample(U, U.pixelSize.width() / 2, h / 2, 0);
  const int v = sample(V, V.pixelSize.width() / 2, h / 2, 0);
  check(std::abs(u - 512) <= 4 && std::abs(v - 512) <= 4, "gray -> neutral chroma (U=V=512)");

  // Monotonic luma across the gradient.
  const int yL = sample(Y, w / 8, h / 2, 0);
  const int yR = sample(Y, 7 * w / 8, h / 2, 0);
  check(yR > yL + 200, "luma increases left->right");
  std::printf("    Y[L]=%d Y[R]=%d  U=%d V=%d  (Y stride=%d u16/px)\n", yL, yR,
              u, v, u16stride(Y));
  enc.release();
}

void testP010(
    QRhi& rhi, const RenderState& state, QRhiTexture* input,
    const std::vector<uint8_t>& px, int w, int h)
{
  std::printf("P010Encoder (semi-planar 4:2:0, high 10 bits):\n");
  P010Encoder enc;
  enc.init(rhi, state, input, w, h);
  uploadAndExec(rhi, enc, input, px);

  const auto& Y = enc.readback(0);  // R16
  const auto& UV = enc.readback(1); // RG16 interleaved
  check(!Y.data.isEmpty() && !UV.data.isEmpty(), "readbacks present");

  // P010 packs into the high 10 bits: the Y component (0) is a multiple of 64.
  bool hi10 = true;
  for(int yy = 0; yy < Y.pixelSize.height() && hi10; ++yy)
    for(int xx = 0; xx < Y.pixelSize.width(); ++xx)
      if((sample(Y, xx, yy, 0) & 0x3F) != 0) { hi10 = false; break; }
  check(hi10, "Y values have low 6 bits zero (10 bits in MSBs)");

  // Neutral chroma (decoded from the high 10 bits): UV is interleaved R=U,G=V.
  const int u = sample(UV, UV.pixelSize.width() / 2, h / 4, 0) >> 6;
  const int v = sample(UV, UV.pixelSize.width() / 2, h / 4, 1) >> 6;
  check(std::abs(u - 512) <= 4 && std::abs(v - 512) <= 4, "gray -> neutral chroma (U=V=512)");

  const int yL = sample(Y, w / 8, h / 2, 0) >> 6;
  const int yR = sample(Y, 7 * w / 8, h / 2, 0) >> 6;
  check(yR > yL + 200, "luma increases left->right");
  std::printf("    Y[L]=%d Y[R]=%d  U=%d V=%d  (UV stride=%d u16/px)\n", yL, yR,
              u, v, u16stride(UV));
  enc.release();
}

void runTests()
{
  const int W = 192, H = 64; // W % 48 == 0, even H
  auto state = createRenderState(GraphicsApi::OpenGL, QSize(W, H), nullptr);
  if(!state || !state->rhi)
  {
    std::printf("ERROR: no QRhi (need a GL-capable display)\n");
    ++g_fail;
    return;
  }
  auto& rhi = *state->rhi;

  // Gray horizontal gradient in an RGBA8 input texture.
  auto* input = rhi.newTexture(
      QRhiTexture::RGBA8, QSize(W, H), 1, QRhiTexture::UsedAsTransferSource);
  input->create();
  std::vector<uint8_t> px(size_t(W) * H * 4);
  for(int y = 0; y < H; ++y)
    for(int x = 0; x < W; ++x)
    {
      uint8_t g = uint8_t(x * 255 / (W - 1));
      uint8_t* p = px.data() + (size_t(y) * W + x) * 4;
      p[0] = p[1] = p[2] = g;
      p[3] = 255;
    }
  // Sanity: upload + read the input back to confirm the gradient is present.
  {
    QRhiReadbackResult irb;
    QRhiCommandBuffer* cb{};
    rhi.beginOffscreenFrame(&cb);
    auto* b = rhi.nextResourceUpdateBatch();
    QRhiTextureSubresourceUploadDescription sub{
        QByteArray(reinterpret_cast<const char*>(px.data()), int(px.size()))};
    b->uploadTexture(input, QRhiTextureUploadDescription{{0, 0, sub}});
    b->readBackTexture(QRhiReadbackDescription{input}, &irb);
    cb->resourceUpdate(b);
    rhi.endOffscreenFrame();
    const auto* ip = reinterpret_cast<const uint8_t*>(irb.data.constData());
    std::printf(
        "input readback: %d bytes  R[0,mid,last]=%d,%d,%d\n", int(irb.data.size()),
        ip ? ip[0] : -1, ip ? ip[(W / 2) * 4] : -1, ip ? ip[(W - 1) * 4] : -1);
  }

  testYUV422P10(rhi, *state, input, px, W, H);
  testP010(rhi, *state, input, px, W, H);

  std::printf("\n%s (%d failures)\n", g_fail ? "FAILED" : "ALL PASS", g_fail);
}
} // namespace

int main(int argc, char** argv)
{
  std::setvbuf(stdout, nullptr, _IONBF, 0); // survive a teardown crash
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  qputenv("SCORE_AUDIO_BACKEND", "dummy");

  score::MinimalGUIApplication app(argc, argv);

  QTimer dialogKiller; // auto-dismiss the package-manager first-run dialog
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
        std::_Exit(g_fail ? 1 : 0); // skip Qt/score teardown (segfaults)
      },
      Qt::QueuedConnection);
  return app.exec();
}
