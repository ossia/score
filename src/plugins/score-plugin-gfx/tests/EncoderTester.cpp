// Offscreen self-test for the GPU video encoders. Runs each encoder on a
// known RGBA gray gradient and checks the readback planes for:
//   - correct 10-bit bit packing (low 10 bits for yuv422p10le, high 10 bits
//     i.e. multiple of 64 for p010),
//   - neutral chroma for a gray input (U == V == 512 in 10-bit),
//   - monotonically increasing luma across the gradient.
// No AJA / libav / gstreamer needed; just a QRhi offscreen context.

#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/encoders/BGRA.hpp>
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

// Planes are packed into RGBA8 (two 16-bit samples per texel), so the readback
// bytes ARE the plane's little-endian uint16 samples, contiguous and tight on
// every backend. Read sample at flat index k.
uint16_t u16at(const QRhiReadbackResult& rb, size_t k)
{
  return reinterpret_cast<const uint16_t*>(rb.data.constData())[k];
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

  // Packed-RGBA8 readback == contiguous LE uint16. Logical sizes: Y = w x h,
  // U/V = w/2 x h (4:2:2).
  auto Yat = [&](int x, int yy) { return u16at(Y, size_t(yy) * w + x); };
  auto Uat = [&](int x, int yy) { return u16at(U, size_t(yy) * (w / 2) + x); };
  auto Vat = [&](int x, int yy) { return u16at(V, size_t(yy) * (w / 2) + x); };

  bool low10 = true;
  for(size_t k = 0, n = size_t(w) * h; k < n && low10; ++k)
    if(u16at(Y, k) > 1023)
      low10 = false;
  check(low10, "Y values fit in low 10 bits (<=1023)");

  const int u = Uat(w / 4, h / 2), v = Vat(w / 4, h / 2);
  check(std::abs(u - 512) <= 4 && std::abs(v - 512) <= 4,
        "gray -> neutral chroma (U=V=512)");

  const int yL = Yat(w / 8, h / 2), yR = Yat(7 * w / 8, h / 2);
  check(yR > yL + 200, "luma increases left->right");
  std::printf("    Y[L]=%d Y[R]=%d  U=%d V=%d\n", yL, yR, u, v);
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

  const auto& Y = enc.readback(0);
  const auto& UV = enc.readback(1);
  check(!Y.data.isEmpty() && !UV.data.isEmpty(), "readbacks present");

  // Packed RGBA8 -> contiguous LE uint16. Y = w x h; UV = (w/2 x h/2),
  // interleaved U,V per site. 10-bit value in the high 10 bits (low 6 zero).
  auto Yat = [&](int x, int yy) { return u16at(Y, size_t(yy) * w + x); };
  auto Uat = [&](int x, int yy) { return u16at(UV, size_t(yy) * w + 2 * x); };
  auto Vat = [&](int x, int yy) { return u16at(UV, size_t(yy) * w + 2 * x + 1); };

  bool hi10 = true;
  for(size_t k = 0, n = size_t(w) * h; k < n && hi10; ++k)
    if((u16at(Y, k) & 0x3F) != 0)
      hi10 = false;
  check(hi10, "Y values have low 6 bits zero (10 bits in MSBs)");

  const int u = Uat(w / 4, h / 4) >> 6, v = Vat(w / 4, h / 4) >> 6;
  check(std::abs(u - 512) <= 4 && std::abs(v - 512) <= 4,
        "gray -> neutral chroma (U=V=512)");

  const int yL = Yat(w / 8, h / 2) >> 6, yR = Yat(7 * w / 8, h / 2) >> 6;
  check(yR > yL + 200, "luma increases left->right");
  std::printf("    Y[L]=%d Y[R]=%d  U=%d V=%d\n", yL, yR, u, v);
  enc.release();
}

// RGB byte-order swizzles. A uniform colored pixel (distinct channels)
// distinguishes the orders that a gray gradient cannot.
void testBGRA(QRhi& rhi, const RenderState& state)
{
  std::printf("BGRAEncoder (RGB byte-order swizzles):\n");
  const int w = 16, h = 4;
  auto* input = rhi.newTexture(
      QRhiTexture::RGBA8, QSize(w, h), 1, QRhiTexture::UsedAsTransferSource);
  input->create();
  std::vector<uint8_t> px(size_t(w) * h * 4);
  for(size_t i = 0; i < px.size(); i += 4)
  {
    px[i] = 11;       // R
    px[i + 1] = 22;   // G
    px[i + 2] = 33;   // B
    px[i + 3] = 255;  // A
  }
  struct Case
  {
    BGRAEncoder::Swizzle s;
    const char* name;
    uint8_t e[4];
  };
  const Case cases[] = {
      {BGRAEncoder::Swizzle::BGRA, "BGRA", {33, 22, 11, 255}},
      {BGRAEncoder::Swizzle::RGBA, "RGBA", {11, 22, 33, 255}},
      {BGRAEncoder::Swizzle::ABGR, "ABGR", {255, 33, 22, 11}},
  };
  for(const auto& c : cases)
  {
    BGRAEncoder enc(c.s);
    enc.init(rhi, state, input, w, h);
    uploadAndExec(rhi, enc, input, px);
    const auto& rb = enc.readback(0);
    const auto* b = reinterpret_cast<const uint8_t*>(rb.data.constData());
    const bool ok = b && b[0] == c.e[0] && b[1] == c.e[1] && b[2] == c.e[2]
                    && b[3] == c.e[3];
    char msg[80];
    std::snprintf(
        msg, sizeof msg, "%s memory order = %d,%d,%d,%d", c.name, c.e[0],
        c.e[1], c.e[2], c.e[3]);
    check(ok, msg);
    enc.release();
  }
  delete input;
}

void runTests()
{
  const int W = 192, H = 64; // W % 48 == 0, even H
  // Backend selectable via SCORE_TEST_API=vulkan|opengl (default OpenGL).
  const QByteArray apiEnv = qgetenv("SCORE_TEST_API").toLower();
  const GraphicsApi api = (apiEnv == "vulkan" || apiEnv == "vk")
                              ? GraphicsApi::Vulkan
                              : GraphicsApi::OpenGL;
  auto state = createRenderState(api, QSize(W, H), nullptr);
  if(!state || !state->rhi)
  {
    std::printf("ERROR: no QRhi (need a GL-capable display)\n");
    ++g_fail;
    return;
  }
  auto& rhi = *state->rhi;
  std::printf("backend=%s\n", rhi.backendName());

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
  testBGRA(rhi, *state);

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
