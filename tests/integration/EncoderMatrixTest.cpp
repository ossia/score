// GPU video encoder matrix test.
//
// Instantiates every encoder in src/plugins/score-plugin-gfx/Gfx/Graph/encoders
// on a real QRhi backend (SCORE_TEST_API=opengl|vulkan, default OpenGL) and
// checks the readback bytes against hand-computed expectations:
//   - UYVY / YUY2 packed byte order (U,Y0,V,Y1 vs Y0,U,Y1,V),
//   - BGRA swizzles incl. ARGB (not covered by EncoderTester),
//   - every PackedRGBEncoder factory (rgb24/bgr24/rgb10/r210be/dpx10be/
//     dpx10le/rgb48/rgb12packed/argb10) with byte-exact red/blue patterns,
//   - v210 field placement (Cb|Y|Cr / Y|Cb|Y / Cr|Y|Cb / Y|Cr|Y) and the
//     padded wire row at width % 6 != 0,
//   - I420 / NV12 plane sizes + U/V (dis)placement,
//   - P010 10-bit-in-MSB placement, YUV422P10 low-10-bit placement,
//   - YUVPlanarEncoder 8/10-bit 4:2:2/4:2:0 factories,
//   - the three ComputeEncoders (BGRA/UYVY/V210) writing to an SSBO,
//   - WireEncoderFactory mapping + wireComputeSupports width gating
//     (v210 width%6 CPU-pack fallback gate),
//   - ColorSpaceOut shader generation branches + BT.709 limited-range
//     white=235/black=16 through a real encoder.
//
// Values assume the default colorMatrixOut() = BT.709 **full range**:
//   red (1,0,0)   -> Y=0.2126  U=0.385428 V=1.0
//   white (1,1,1) -> Y=1.0     U=V=0.5
//   black (0,0,0) -> Y=0.0     U=V=0.5
// 8-bit:  Y_red=54  U_red=98  V_red=255, chroma@0.5 = 127|128 (tie)
// 10-bit: Y_red=217 U_red=394 V_red=1023, chroma@0.5 = 511|512
//
// Vertical orientation: the encoders' flip_y converts the render-graph's
// GL-style Y-up content to video top-down order. A texture uploaded via
// QRhi (top-left origin) goes through the same flip, so readback row r
// corresponds to UPLOADED row (H-1-r) on OpenGL and Vulkan alike (both
// compile the GLSL flip branch). Verified explicitly in the orientation
// tests below.

#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/encoders/BGRA.hpp>
#include <Gfx/Graph/encoders/BGRACompute.hpp>
#include <Gfx/Graph/encoders/I420.hpp>
#include <Gfx/Graph/encoders/NV12.hpp>
#include <Gfx/Graph/encoders/P010.hpp>
#include <Gfx/Graph/encoders/PackedRGB.hpp>
#include <Gfx/Graph/encoders/UYVY.hpp>
#include <Gfx/Graph/encoders/UYVYCompute.hpp>
#include <Gfx/Graph/encoders/V210.hpp>
#include <Gfx/Graph/encoders/V210Compute.hpp>
#include <Gfx/Graph/encoders/WireEncoderFactory.hpp>
#include <Gfx/Graph/encoders/YUV422P10.hpp>
#include <Gfx/Graph/encoders/YUVPlanar.hpp>
#include <Gfx/Graph/encoders/YUY2.hpp>

#include <core/application/MinimalApplication.hpp>

#include <QGuiApplication>

#include <private/qrhi_p.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

using namespace score::gfx;

namespace
{
// ---------------------------------------------------------------------------
// QRhi context (one per process). GL needs a windowing system (xcb locally),
// which is why this test is registered GUI / labelled "gui".
// ---------------------------------------------------------------------------
// createRenderState() reads score::AppContext().settings<Gfx::Settings::Model>()
// so a plain QGuiApplication is not enough: bring up the same minimal score
// application EncoderTester uses. It is deliberately LEAKED: score teardown
// crashes (see EncoderTester's std::_Exit) and Catch2 must return from main
// normally for the coverage profile to be written.
void ensureApp()
{
  if(!QCoreApplication::instance())
  {
    qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
    qputenv("SCORE_AUDIO_BACKEND", "dummy");
    static int argc = 1;
    static char arg0[] = "EncoderMatrixTest";
    static char* argv[] = {arg0, nullptr};
    new score::MinimalGUIApplication(argc, argv, /*show=*/false);
  }
}

std::shared_ptr<RenderState>& stateHolder()
{
  static std::shared_ptr<RenderState> state;
  return state;
}

RenderState* renderState()
{
  static bool tried = false;
  if(!tried)
  {
    tried = true;
    ensureApp();
    const QByteArray apiEnv = qgetenv("SCORE_TEST_API").toLower();
    const GraphicsApi api = (apiEnv == "vulkan" || apiEnv == "vk")
                                ? GraphicsApi::Vulkan
                                : GraphicsApi::OpenGL;
    auto st = createRenderState(api, QSize(64, 64), nullptr);
    if(st && st->rhi && st->api != GraphicsApi::Null)
      stateHolder() = st;
  }
  return stateHolder().get();
}

struct Ctx
{
  RenderState* state{};
  QRhi* rhi{};
};

#define REQUIRE_GPU_CTX()                                             \
  Ctx ctx;                                                            \
  do                                                                  \
  {                                                                   \
    ctx.state = renderState();                                        \
    if(!ctx.state)                                                    \
      SKIP("no GL/Vulkan-capable QRhi available on this machine");    \
    ctx.rhi = ctx.state->rhi;                                         \
  } while(0)

// ---------------------------------------------------------------------------
// Pixel helpers
// ---------------------------------------------------------------------------
struct Rgba
{
  uint8_t r, g, b, a;
};
constexpr Rgba RED{255, 0, 0, 255};
constexpr Rgba BLUE{0, 0, 255, 255};
constexpr Rgba WHITE{255, 255, 255, 255};
constexpr Rgba BLACK{0, 0, 0, 255};

std::vector<uint8_t> solid(int w, int h, Rgba c)
{
  std::vector<uint8_t> px(size_t(w) * h * 4);
  for(size_t i = 0; i < px.size(); i += 4)
  {
    px[i] = c.r;
    px[i + 1] = c.g;
    px[i + 2] = c.b;
    px[i + 3] = c.a;
  }
  return px;
}

// Left half `l`, right half `r`.
std::vector<uint8_t> halves(int w, int h, Rgba l, Rgba r)
{
  std::vector<uint8_t> px(size_t(w) * h * 4);
  for(int y = 0; y < h; y++)
    for(int x = 0; x < w; x++)
    {
      const Rgba& c = (x < w / 2) ? l : r;
      uint8_t* p = px.data() + (size_t(y) * w + x) * 4;
      p[0] = c.r;
      p[1] = c.g;
      p[2] = c.b;
      p[3] = c.a;
    }
  return px;
}

// Even columns `e`, odd columns `o`.
std::vector<uint8_t> columns(int w, int h, Rgba e, Rgba o)
{
  std::vector<uint8_t> px(size_t(w) * h * 4);
  for(int y = 0; y < h; y++)
    for(int x = 0; x < w; x++)
    {
      const Rgba& c = (x % 2 == 0) ? e : o;
      uint8_t* p = px.data() + (size_t(y) * w + x) * 4;
      p[0] = c.r;
      p[1] = c.g;
      p[2] = c.b;
      p[3] = c.a;
    }
  return px;
}

// Row y = rows[y].
std::vector<uint8_t> rowPattern(int w, const std::vector<Rgba>& rows)
{
  const int h = int(rows.size());
  std::vector<uint8_t> px(size_t(w) * h * 4);
  for(int y = 0; y < h; y++)
    for(int x = 0; x < w; x++)
    {
      uint8_t* p = px.data() + (size_t(y) * w + x) * 4;
      p[0] = rows[y].r;
      p[1] = rows[y].g;
      p[2] = rows[y].b;
      p[3] = rows[y].a;
    }
  return px;
}

bool near(int a, int b, int tol = 1)
{
  return std::abs(a - b) <= tol;
}

// ---------------------------------------------------------------------------
// Encoder runners
// ---------------------------------------------------------------------------
QRhiTexture* makeInput(QRhi& rhi, int w, int h)
{
  auto* input = rhi.newTexture(
      QRhiTexture::RGBA8, QSize(w, h), 1, QRhiTexture::UsedAsTransferSource);
  REQUIRE(input->create());
  return input;
}

// Upload + run a fragment-shader encoder in one offscreen frame (the upload
// must be submitted before the encoder's passes sample it).
void runFragment(
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

// Upload + run a compute encoder + read the SSBO back, in one offscreen frame.
QByteArray runCompute(
    QRhi& rhi, ComputeEncoder& enc, QRhiTexture* input,
    const std::vector<uint8_t>& px, QRhiBuffer* out, int outSize)
{
  QRhiCommandBuffer* cb{};
  rhi.beginOffscreenFrame(&cb);
  auto* batch = rhi.nextResourceUpdateBatch();
  QRhiTextureSubresourceUploadDescription sub{
      QByteArray(reinterpret_cast<const char*>(px.data()), int(px.size()))};
  batch->uploadTexture(input, QRhiTextureUploadDescription{{0, 0, sub}});
  cb->resourceUpdate(batch);
  enc.exec(rhi, *cb, rhi.nextResourceUpdateBatch());
  // QRhiBufferReadbackResult: distinct type in Qt <= 6.5, alias of
  // QRhiReadbackResult afterwards.
  QRhiBufferReadbackResult rb;
  auto* batch2 = rhi.nextResourceUpdateBatch();
  batch2->readBackBuffer(out, 0, outSize, &rb);
  cb->resourceUpdate(batch2);
  rhi.endOffscreenFrame();
  return rb.data;
}

const uint8_t* bytes(const QRhiReadbackResult& rb)
{
  return reinterpret_cast<const uint8_t*>(rb.data.constData());
}
const uint16_t* words16(const QRhiReadbackResult& rb)
{
  return reinterpret_cast<const uint16_t*>(rb.data.constData());
}
uint32_t word32(const QByteArray& data, size_t idx)
{
  return reinterpret_cast<const uint32_t*>(data.constData())[idx];
}

// v210 32-bit word -> (low, mid, high) 10-bit fields.
std::array<int, 3> v210fields(uint32_t w)
{
  return {int(w & 0x3FF), int((w >> 10) & 0x3FF), int((w >> 20) & 0x3FF)};
}

void checkV210Fields(uint32_t w, int lo, int mid, int hi, const char* what)
{
  auto f = v210fields(w);
  INFO(
      what << ": word=0x" << std::hex << w << std::dec << " fields=" << f[0]
           << "," << f[1] << "," << f[2] << " expected=" << lo << "," << mid
           << "," << hi);
  CHECK(near(f[0], lo));
  CHECK(near(f[1], mid));
  CHECK(near(f[2], hi));
}

// BT.709 full-range constants for red (1,0,0), see file header.
constexpr int Y_RED_8 = 54, U_RED_8 = 98, V_RED_8 = 255;
constexpr int Y_RED_10 = 217, U_RED_10 = 394, V_RED_10 = 1023;
} // namespace

// ===========================================================================
// Pure-logic coverage: WireEncoderFactory + ColorSpaceOut (no GPU needed)
// ===========================================================================
TEST_CASE("WireEncoderFactory maps every wired format", "[gfx][encoders]")
{
  using F = score::gfx::interop::VideoPixelFormat;

  const F wired[] = {F::UYVY422, F::YUYV422, F::V210,    F::BGRA8,
                     F::RGBA8,   F::ARGB8,   F::ABGR8,   F::RGB24,
                     F::BGR24,   F::R210,    F::RGB10,   F::ARGB10,
                     F::DPX10,   F::DPX10LE, F::RGB12P,  F::RGB48,
                     F::YUV422P10, F::YUV422P, F::YUV420P, F::YUV420P10,
                     F::NV12,    F::P010};
  for(F f : wired)
  {
    INFO("format " << int(f));
    CHECK(makeWireEncoder(f) != nullptr);
  }

  const F unwired[]
      = {F::Unknown, F::P210, F::Mono8, F::RGBA16F, F::V216, F::BayerRG8};
  for(F f : unwired)
  {
    INFO("format " << int(f));
    CHECK(makeWireEncoder(f) == nullptr);
  }

  // Compute variants exist only for V210 / UYVY / BGRA.
  CHECK(makeWireComputeEncoder(F::V210) != nullptr);
  CHECK(makeWireComputeEncoder(F::UYVY422) != nullptr);
  CHECK(makeWireComputeEncoder(F::BGRA8) != nullptr);
  CHECK(makeWireComputeEncoder(F::YUYV422) == nullptr);
  CHECK(makeWireComputeEncoder(F::NV12) == nullptr);

  // Width gating: v210 compute requires width % 6 == 0 (the CPU-pack
  // fallback gate: 1280 / 2048-DCI go through UYVY + CPU packing instead).
  CHECK(wireComputeSupports(F::V210, 1920));
  CHECK(wireComputeSupports(F::V210, 48));
  CHECK_FALSE(wireComputeSupports(F::V210, 1280));
  CHECK_FALSE(wireComputeSupports(F::V210, 16));
  CHECK(wireComputeSupports(F::UYVY422, 1280));
  CHECK_FALSE(wireComputeSupports(F::UYVY422, 1281));
  CHECK(wireComputeSupports(F::BGRA8, 1281));
  CHECK_FALSE(wireComputeSupports(F::NV12, 1920));
}

TEST_CASE("ColorSpaceOut shader generation branches", "[gfx][encoders]")
{
  // Identity / RGB passthrough
  auto rgb = colorMatrixOut(
      AVCOL_SPC_RGB, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709);
  CHECK(rgb.contains("return rgb;"));

  // BT.709 full vs limited pick different matrices
  auto bt709f = colorMatrixOut(
      AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709);
  auto bt709l = colorMatrixOut(
      AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_MPEG, AVCOL_PRI_BT709);
  CHECK(bt709f.contains("0.2126"));
  CHECK(bt709l.contains("0.182586"));
  CHECK(bt709f != bt709l);

  // BT.601 via both aliases
  auto bt601a = colorMatrixOut(
      AVCOL_SPC_BT470BG, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709);
  auto bt601b = colorMatrixOut(
      AVCOL_SPC_SMPTE170M, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709);
  CHECK(bt601a.contains("0.299"));
  CHECK(bt601a == bt601b);

  // SMPTE240M: dedicated full-range matrix, BT.709 fallback for limited
  auto s240f = colorMatrixOut(
      AVCOL_SPC_SMPTE240M, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709);
  auto s240l = colorMatrixOut(
      AVCOL_SPC_SMPTE240M, AVCOL_TRC_BT709, AVCOL_RANGE_MPEG, AVCOL_PRI_BT709);
  CHECK(s240f.contains("0.2122"));
  CHECK(s240l.contains("0.182586"));

  // BT.2020 PQ / HLG / SDR
  auto pq = colorMatrixOut(
      AVCOL_SPC_BT2020_NCL, AVCOL_TRC_SMPTE2084, AVCOL_RANGE_MPEG,
      AVCOL_PRI_BT2020);
  CHECK(pq.contains("pqOetf"));
  auto hlg = colorMatrixOut(
      AVCOL_SPC_BT2020_NCL, AVCOL_TRC_ARIB_STD_B67, AVCOL_RANGE_MPEG,
      AVCOL_PRI_BT2020);
  CHECK(hlg.contains("hlgOetf"));
  auto sdr2020 = colorMatrixOut(
      AVCOL_SPC_BT2020_NCL, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT2020);
  CHECK(sdr2020.contains("0.2627"));

  // Unknown space falls back on primaries (BT.2020 -> PQ/HLG/SDR)
  auto fbPq = colorMatrixOut(
      AVCOL_SPC_UNSPECIFIED, AVCOL_TRC_SMPTE2084, AVCOL_RANGE_MPEG,
      AVCOL_PRI_BT2020);
  CHECK(fbPq.contains("pqOetf"));
  auto fbHlg = colorMatrixOut(
      AVCOL_SPC_UNSPECIFIED, AVCOL_TRC_ARIB_STD_B67, AVCOL_RANGE_MPEG,
      AVCOL_PRI_BT2020);
  CHECK(fbHlg.contains("hlgOetf"));
  auto fbSdr = colorMatrixOut(
      AVCOL_SPC_UNSPECIFIED, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG,
      AVCOL_PRI_BT2020);
  CHECK(fbSdr.contains("0.2627"));

  // Ultimate fallback: BT.709
  auto fb709 = colorMatrixOut(
      AVCOL_SPC_UNSPECIFIED, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709);
  CHECK(fb709 == bt709f);

  // input_trc variants route through the transfer-function shaders
  auto lin = colorMatrixOut(
      AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709,
      AVCOL_TRC_LINEAR);
  CHECK(lin.contains("srgbOetfOut"));
  auto pqIn = colorMatrixOut(
      AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709,
      AVCOL_TRC_SMPTE2084);
  CHECK(pqIn.contains("pqEotf"));
  auto hlgIn = colorMatrixOut(
      AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709,
      AVCOL_TRC_ARIB_STD_B67);
  CHECK(hlgIn.contains("hlgEotf"));

  // Default-argument overload == explicit BT.709 full sRGB-in
  CHECK(colorMatrixOut() == bt709f);

  // HDR out with the input-trc matrix: all four bt2020 PQ input branches
  for(auto trc : {AVCOL_TRC_UNSPECIFIED, AVCOL_TRC_SMPTE2084,
                  AVCOL_TRC_ARIB_STD_B67, AVCOL_TRC_LINEAR})
  {
    auto s = colorMatrixOut(
        AVCOL_SPC_BT2020_NCL, AVCOL_TRC_SMPTE2084, AVCOL_RANGE_MPEG,
        AVCOL_PRI_BT2020, trc);
    CHECK(s.contains("convert_from_rgb"));
    auto h = colorMatrixOut(
        AVCOL_SPC_BT2020_NCL, AVCOL_TRC_ARIB_STD_B67, AVCOL_RANGE_MPEG,
        AVCOL_PRI_BT2020, trc);
    CHECK(h.contains("convert_from_rgb"));
    auto sdr = colorMatrixOut(
        AVCOL_SPC_BT2020_NCL, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG,
        AVCOL_PRI_BT2020, trc);
    CHECK(sdr.contains("convert_from_rgb"));
  }
}

// ===========================================================================
// Packed 4:2:2 — UYVY vs YUY2 byte order
// ===========================================================================
TEST_CASE("UYVYEncoder packs U,Y0,V,Y1", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  auto& rhi = *ctx.rhi;
  const int W = 8, H = 2;
  auto* input = makeInput(rhi, W, H);
  // even px black, odd px white: Y0=0, Y1=255, U=V=127|128
  auto px = columns(W, H, BLACK, WHITE);

  UYVYEncoder enc;
  enc.init(rhi, *ctx.state, input, W, H);
  CHECK(enc.planeCount() == 1);
  CHECK(enc.outputTexture() != nullptr);
  runFragment(rhi, enc, input, px);

  const auto& rb = enc.readback(0);
  REQUIRE(rb.data.size() == W / 2 * H * 4);
  const uint8_t* b = bytes(rb);
  for(int t = 0; t < W / 2 * H; t++)
  {
    INFO("texel " << t);
    CHECK(near(b[t * 4 + 0], 128, 1)); // U
    CHECK(b[t * 4 + 1] == 0);          // Y0 (black)
    CHECK(near(b[t * 4 + 2], 128, 1)); // V
    CHECK(b[t * 4 + 3] == 255);        // Y1 (white)
  }

  // setReadbackEnabled(false) branch: exec must not touch the readback.
  enc.setReadbackEnabled(false);
  QRhiCommandBuffer* cb{};
  rhi.beginOffscreenFrame(&cb);
  enc.exec(rhi, *cb);
  rhi.endOffscreenFrame();

  enc.release();
  delete input;
}

TEST_CASE("YUY2Encoder packs Y0,U,Y1,V", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  auto& rhi = *ctx.rhi;
  const int W = 8, H = 2;
  auto* input = makeInput(rhi, W, H);
  auto px = columns(W, H, BLACK, WHITE);

  YUY2Encoder enc;
  enc.init(rhi, *ctx.state, input, W, H);
  CHECK(enc.planeCount() == 1);
  runFragment(rhi, enc, input, px);

  const auto& rb = enc.readback(0);
  REQUIRE(rb.data.size() == W / 2 * H * 4);
  const uint8_t* b = bytes(rb);
  for(int t = 0; t < W / 2 * H; t++)
  {
    INFO("texel " << t);
    CHECK(b[t * 4 + 0] == 0);          // Y0
    CHECK(near(b[t * 4 + 1], 128, 1)); // U
    CHECK(b[t * 4 + 2] == 255);        // Y1
    CHECK(near(b[t * 4 + 3], 128, 1)); // V
  }
  enc.release();
  delete input;
}

TEST_CASE("UYVY BT.709 limited range: white=235 black=16", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  auto& rhi = *ctx.rhi;
  const int W = 8, H = 2;
  auto* input = makeInput(rhi, W, H);
  auto px = columns(W, H, BLACK, WHITE);

  UYVYEncoder enc;
  enc.init(
      rhi, *ctx.state, input, W, H,
      colorMatrixOut(
          AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_MPEG, AVCOL_PRI_BT709));
  runFragment(rhi, enc, input, px);

  const auto& rb = enc.readback(0);
  REQUIRE(rb.data.size() == W / 2 * H * 4);
  const uint8_t* b = bytes(rb);
  // limited range: Y in [16,235], chroma neutral at 128
  CHECK(near(b[1], 16, 1));  // Y0 = black
  CHECK(near(b[3], 235, 1)); // Y1 = white
  CHECK(near(b[0], 128, 1)); // U
  CHECK(near(b[2], 128, 1)); // V
  enc.release();
  delete input;
}

// ===========================================================================
// BGRA swizzles + vertical orientation
// ===========================================================================
TEST_CASE("BGRAEncoder all four swizzles", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  auto& rhi = *ctx.rhi;
  const int W = 4, H = 2;
  auto* input = makeInput(rhi, W, H);
  auto px = solid(W, H, Rgba{11, 22, 33, 200});

  struct Case
  {
    BGRAEncoder::Swizzle s;
    const char* name;
    uint8_t e[4];
  };
  const Case cases[] = {
      {BGRAEncoder::Swizzle::BGRA, "BGRA", {33, 22, 11, 200}},
      {BGRAEncoder::Swizzle::RGBA, "RGBA", {11, 22, 33, 200}},
      {BGRAEncoder::Swizzle::ABGR, "ABGR", {200, 33, 22, 11}},
      {BGRAEncoder::Swizzle::ARGB, "ARGB", {200, 11, 22, 33}},
  };
  for(const auto& c : cases)
  {
    BGRAEncoder enc(c.s);
    enc.init(rhi, *ctx.state, input, W, H);
    runFragment(rhi, enc, input, px);
    const auto& rb = enc.readback(0);
    REQUIRE(rb.data.size() == W * H * 4);
    const uint8_t* b = bytes(rb);
    for(int i = 0; i < W * H; i++)
    {
      INFO(c.name << " texel " << i);
      CHECK(b[i * 4 + 0] == c.e[0]);
      CHECK(b[i * 4 + 1] == c.e[1]);
      CHECK(b[i * 4 + 2] == c.e[2]);
      CHECK(b[i * 4 + 3] == c.e[3]);
    }
    enc.release();
  }

  // setReadbackEnabled(false) / outputTexture() branch.
  {
    BGRAEncoder enc(BGRAEncoder::Swizzle::BGRA);
    enc.init(rhi, *ctx.state, input, W, H);
    CHECK(enc.outputTexture() != nullptr);
    CHECK(enc.planeCount() == 1);
    enc.setReadbackEnabled(false);
    QRhiCommandBuffer* cb{};
    rhi.beginOffscreenFrame(&cb);
    enc.exec(rhi, *cb);
    rhi.endOffscreenFrame();
    enc.release();
  }
  delete input;
}

TEST_CASE("BGRAEncoder vertical orientation (uploaded texture)", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  auto& rhi = *ctx.rhi;
  const int W = 4, H = 2;
  auto* input = makeInput(rhi, W, H);
  // uploaded row 0 = red, row 1 = blue
  auto px = rowPattern(W, {RED, BLUE});

  BGRAEncoder enc(BGRAEncoder::Swizzle::RGBA);
  enc.init(rhi, *ctx.state, input, W, H);
  runFragment(rhi, enc, input, px);
  const auto& rb = enc.readback(0);
  REQUIRE(rb.data.size() == W * H * 4);
  const uint8_t* b = bytes(rb);
  // The encoder's flip_y converts render-graph GL Y-up content to video
  // top-down order; an uploaded (top-left-origin) texture therefore comes
  // out vertically flipped: readback row 0 == uploaded row H-1.
  INFO("readback row0 rgba=" << int(b[0]) << "," << int(b[1]) << "," << int(b[2]));
  CHECK(b[0] == 0); // row 0 == uploaded row 1 == blue
  CHECK(b[2] == 255);
  const uint8_t* r1 = b + W * 4;
  CHECK(r1[0] == 255); // row 1 == uploaded row 0 == red
  CHECK(r1[2] == 0);
  enc.release();
  delete input;
}

// ===========================================================================
// PackedRGB — all 9 factories, byte-exact
// ===========================================================================
namespace
{
// Run a PackedRGBEncoder on a red|blue half-split and compare a full row
// against per-pixel expected byte groups.
void checkPackedRGB(
    Ctx& ctx, std::unique_ptr<PackedRGBEncoder> enc, const char* name,
    int bytesPerPx, const std::vector<uint8_t>& redBytes,
    const std::vector<uint8_t>& blueBytes)
{
  auto& rhi = *ctx.rhi;
  const int W = 8, H = 2;
  auto* input = makeInput(rhi, W, H);
  auto px = halves(W, H, RED, BLUE);

  enc->init(rhi, *ctx.state, input, W, H);
  CHECK(enc->planeCount() == 1);
  runFragment(rhi, *enc, input, px);

  const auto& rb = enc->readback(0);
  const int rowBytes = W * bytesPerPx;
  REQUIRE(rb.data.size() == rowBytes * H);
  const uint8_t* b = bytes(rb);
  for(int row = 0; row < H; row++)
  {
    for(int x = 0; x < W; x++)
    {
      const auto& e = (x < W / 2) ? redBytes : blueBytes;
      for(int k = 0; k < bytesPerPx; k++)
      {
        INFO(
            name << " row " << row << " px " << x << " byte " << k << ": got "
                 << int(b[row * rowBytes + x * bytesPerPx + k]) << " expected "
                 << int(e[k]));
        CHECK(b[row * rowBytes + x * bytesPerPx + k] == e[k]);
      }
    }
  }
  enc->release();
  delete input;
}

// Same, for the 2-pixels-per-9-bytes 12-bit packed format.
void checkPackedRGBGroup9(
    Ctx& ctx, std::unique_ptr<PackedRGBEncoder> enc, const char* name,
    const std::vector<uint8_t>& redGroup, const std::vector<uint8_t>& blueGroup)
{
  auto& rhi = *ctx.rhi;
  const int W = 8, H = 2;
  auto* input = makeInput(rhi, W, H);
  auto px = halves(W, H, RED, BLUE);

  enc->init(rhi, *ctx.state, input, W, H);
  runFragment(rhi, *enc, input, px);

  const auto& rb = enc->readback(0);
  const int rowBytes = W * 9 / 2; // 36
  REQUIRE(rb.data.size() == rowBytes * H);
  const uint8_t* b = bytes(rb);
  for(int row = 0; row < H; row++)
  {
    for(int g = 0; g < W / 2; g++) // pixel pairs (0,1) (2,3) (4,5) (6,7)
    {
      const auto& e = (g < W / 4) ? redGroup : blueGroup;
      for(int k = 0; k < 9; k++)
      {
        INFO(
            name << " row " << row << " group " << g << " byte " << k
                 << ": got " << int(b[row * rowBytes + g * 9 + k])
                 << " expected " << int(e[k]));
        CHECK(b[row * rowBytes + g * 9 + k] == e[k]);
      }
    }
  }
  enc->release();
  delete input;
}
}

TEST_CASE("PackedRGBEncoder rgb24 / bgr24", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  checkPackedRGB(
      ctx, PackedRGBEncoder::rgb24(), "rgb24", 3, {255, 0, 0}, {0, 0, 255});
  checkPackedRGB(
      ctx, PackedRGBEncoder::bgr24(), "bgr24", 3, {0, 0, 255}, {255, 0, 0});
}

TEST_CASE("PackedRGBEncoder 10-bit RGB words", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  // AJA NTV2_FBF_10BIT_RGB: w = R | G<<10 | B<<20, little-endian.
  // red: w=0x000003FF -> LE FF 03 00 00 ; blue: w=0x3FF00000 -> 00 00 F0 3F
  checkPackedRGB(
      ctx, PackedRGBEncoder::rgb10(), "rgb10", 4, {0xFF, 0x03, 0x00, 0x00},
      {0x00, 0x00, 0xF0, 0x3F});
  // DeckLink r210: w = R<<20 | G<<10 | B, big-endian.
  // red: w=0x3FF00000 -> BE 3F F0 00 00 ; blue: w=0x000003FF -> 00 00 03 FF
  checkPackedRGB(
      ctx, PackedRGBEncoder::r210be(), "r210be", 4, {0x3F, 0xF0, 0x00, 0x00},
      {0x00, 0x00, 0x03, 0xFF});
  // DPX: v = R<<22 | G<<12 | B<<2.
  // red: v=0xFFC00000 ; blue: v=0x00000FFC
  checkPackedRGB(
      ctx, PackedRGBEncoder::dpx10be(), "dpx10be", 4, {0xFF, 0xC0, 0x00, 0x00},
      {0x00, 0x00, 0x0F, 0xFC});
  checkPackedRGB(
      ctx, PackedRGBEncoder::dpx10le(), "dpx10le", 4, {0x00, 0x00, 0xC0, 0xFF},
      {0xFC, 0x0F, 0x00, 0x00});
}

TEST_CASE("PackedRGBEncoder rgb48 / argb10 / rgb12packed", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  // 48-bit RGB: R16,G16,B16 little-endian, full 16-bit scale.
  checkPackedRGB(
      ctx, PackedRGBEncoder::rgb48(), "rgb48", 6,
      {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00},
      {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF});
  // NTV2_FBF_10BIT_ARGB: B,G,R,A 10-bit packed into 5 bytes, A=1023.
  checkPackedRGB(
      ctx, PackedRGBEncoder::argb10(), "argb10", 5,
      {0x00, 0x00, 0xF0, 0xFF, 0xFF}, {0xFF, 0x03, 0x00, 0xC0, 0xFF});
  // NTV2_FBF_12BIT_RGB_PACKED: 2 px in 9 bytes, values left-justified <<4.
  checkPackedRGBGroup9(
      ctx, PackedRGBEncoder::rgb12packed(), "rgb12packed",
      {0xFF, 0xF0, 0x00, 0x00, 0x0F, 0xFF, 0x00, 0x00, 0x00},
      {0x00, 0x00, 0x00, 0xFF, 0xF0, 0x00, 0x00, 0x0F, 0xFF});
}

// ===========================================================================
// v210 — field placement + padded wire row
// ===========================================================================
TEST_CASE("V210Encoder field placement (width % 6 == 0)", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  auto& rhi = *ctx.rhi;
  const int W = 12, H = 2;
  auto* input = makeInput(rhi, W, H);
  auto px = solid(W, H, RED);

  V210Encoder enc;
  enc.init(rhi, *ctx.state, input, W, H);
  CHECK(enc.planeCount() == 1);
  CHECK(enc.outputTexture() != nullptr);
  runFragment(rhi, enc, input, px);

  const auto& rb = enc.readback(0);
  // Padded wire row: ((12+47)/48)*128 = 128 bytes = 32 words per row.
  const int wordsPerRow = 32;
  REQUIRE(rb.data.size() == wordsPerRow * 4 * H);

  for(int row = 0; row < H; row++)
  {
    const size_t base = size_t(row) * wordsPerRow;
    // Meaningful words: 2 groups of 4 for 12 px.
    for(int g = 0; g < 2; g++)
    {
      checkV210Fields(
          word32(rb.data, base + g * 4 + 0), U_RED_10, Y_RED_10, V_RED_10,
          "w0 = Cb|Y0|Cr");
      checkV210Fields(
          word32(rb.data, base + g * 4 + 1), Y_RED_10, U_RED_10, Y_RED_10,
          "w1 = Y1|Cb|Y2");
      checkV210Fields(
          word32(rb.data, base + g * 4 + 2), V_RED_10, Y_RED_10, U_RED_10,
          "w2 = Cr|Y3|Cb");
      checkV210Fields(
          word32(rb.data, base + g * 4 + 3), Y_RED_10, V_RED_10, Y_RED_10,
          "w3 = Y4|Cr|Y5");
    }
  }

  // setReadbackEnabled(false) branch: exec without scheduling the readback.
  enc.setReadbackEnabled(false);
  {
    QRhiCommandBuffer* cb{};
    rhi.beginOffscreenFrame(&cb);
    enc.exec(rhi, *cb);
    rhi.endOffscreenFrame();
  }

  enc.release();
  delete input;
}

TEST_CASE("V210Encoder width % 6 != 0 pads the wire row", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  auto& rhi = *ctx.rhi;
  const int W = 16, H = 2; // 16 % 6 == 4: tail group edge-clamps
  auto* input = makeInput(rhi, W, H);
  auto px = solid(W, H, RED);

  V210Encoder enc;
  enc.init(rhi, *ctx.state, input, W, H);
  runFragment(rhi, enc, input, px);

  const auto& rb = enc.readback(0);
  const int wordsPerRow = 32; // ((16+47)/48)*128 bytes = 128
  REQUIRE(rb.data.size() == wordsPerRow * 4 * H);
  // Constant red: even the clamped tail/padding words carry the same fields.
  for(int row = 0; row < H; row++)
  {
    const size_t base = size_t(row) * wordsPerRow;
    checkV210Fields(
        word32(rb.data, base + 0), U_RED_10, Y_RED_10, V_RED_10, "w0");
    // Tail group (pixels 12..15 + clamped 16,17):
    checkV210Fields(
        word32(rb.data, base + 8), U_RED_10, Y_RED_10, V_RED_10, "tail w0");
    checkV210Fields(
        word32(rb.data, base + 11), Y_RED_10, V_RED_10, Y_RED_10, "tail w3");
  }
  enc.release();
  delete input;
}

// ===========================================================================
// Planar / semi-planar
// ===========================================================================
TEST_CASE("I420Encoder plane sizes and U/V placement", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  auto& rhi = *ctx.rhi;
  const int W = 8, H = 4;
  auto* input = makeInput(rhi, W, H);
  auto px = solid(W, H, RED);

  I420Encoder enc;
  enc.init(rhi, *ctx.state, input, W, H);
  CHECK(enc.planeCount() == 3);
  // Base-class defaults (multi-plane encoders have no single output texture,
  // setReadbackEnabled is a no-op).
  CHECK(enc.outputTexture() == nullptr);
  enc.setReadbackEnabled(false);
  enc.setReadbackEnabled(true);
  runFragment(rhi, enc, input, px);

  const auto& Y = enc.readback(0);
  const auto& U = enc.readback(1);
  const auto& V = enc.readback(2);
  REQUIRE(Y.data.size() == W * H);
  REQUIRE(U.data.size() == (W / 2) * (H / 2));
  REQUIRE(V.data.size() == (W / 2) * (H / 2));
  for(int i = 0; i < W * H; i++)
  {
    INFO("Y sample " << i);
    CHECK(near(bytes(Y)[i], Y_RED_8));
  }
  for(int i = 0; i < (W / 2) * (H / 2); i++)
  {
    INFO("chroma sample " << i);
    CHECK(near(bytes(U)[i], U_RED_8)); // U != V for red: catches a U/V swap
    CHECK(bytes(V)[i] == V_RED_8);
  }
  enc.release();
  delete input;
}

TEST_CASE("NV12Encoder UV interleave order", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  auto& rhi = *ctx.rhi;
  const int W = 8, H = 4;
  auto* input = makeInput(rhi, W, H);
  auto px = solid(W, H, RED);

  NV12Encoder enc;
  enc.init(rhi, *ctx.state, input, W, H);
  CHECK(enc.planeCount() == 2);
  runFragment(rhi, enc, input, px);

  const auto& Y = enc.readback(0);
  const auto& UV = enc.readback(1);
  REQUIRE(Y.data.size() == W * H);
  // NV12 wire layout: tight 2 bytes per chroma site on EVERY Qt/backend
  // combination. On Qt >= 6.10 the RG8 target reads back tightly; on
  // Qt < 6.10 NV12Encoder renders the interleaved U,V bytes into an R8
  // target (GL reads R8 back tightly on all versions) — same byte layout.
  const int sites = (W / 2) * (H / 2);
  REQUIRE(UV.data.size() == sites * 2);
  for(int i = 0; i < W * H; i++)
  {
    INFO("Y sample " << i);
    CHECK(near(bytes(Y)[i], Y_RED_8));
  }
  for(int i = 0; i < sites; i++)
  {
    INFO("UV site " << i);
    CHECK(near(bytes(UV)[i * 2 + 0], U_RED_8)); // U first
    CHECK(bytes(UV)[i * 2 + 1] == V_RED_8);     // then V
  }
  enc.release();
  delete input;
}

TEST_CASE("P010Encoder 10-bit values sit in the high bits", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  auto& rhi = *ctx.rhi;
  const int W = 8, H = 4;
  auto* input = makeInput(rhi, W, H);
  auto px = solid(W, H, RED);

  P010Encoder enc;
  enc.init(rhi, *ctx.state, input, W, H);
  CHECK(enc.planeCount() == 2);
  runFragment(rhi, enc, input, px);

  const auto& Y = enc.readback(0);
  const auto& UV = enc.readback(1);
  // Same byte layout on both P010 implementations (Qt >= 6.10 R16/RG16 or
  // the RGBA8-packed fallback): Y = w*h LE uint16, UV = interleaved.
  REQUIRE(Y.data.size() == W * H * 2);
  REQUIRE(UV.data.size() == (W / 2) * (H / 2) * 2 * 2);
  const uint16_t* y = words16(Y);
  const uint16_t* uv = words16(UV);
  for(int i = 0; i < W * H; i++)
  {
    INFO("Y sample " << i << " = " << y[i]);
    CHECK((y[i] & 0x3F) == 0); // MSB placement: low 6 bits zero
    CHECK(near(y[i] >> 6, Y_RED_10));
  }
  for(int i = 0; i < (W / 2) * (H / 2); i++)
  {
    INFO("UV site " << i);
    CHECK((uv[i * 2] & 0x3F) == 0);
    CHECK((uv[i * 2 + 1] & 0x3F) == 0);
    CHECK(near(uv[i * 2 + 0] >> 6, U_RED_10)); // U first
    CHECK(near(uv[i * 2 + 1] >> 6, V_RED_10)); // then V
  }
  enc.release();
  delete input;
}

TEST_CASE("YUV422P10Encoder 10-bit values sit in the low bits", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  auto& rhi = *ctx.rhi;
  const int W = 8, H = 2;
  auto* input = makeInput(rhi, W, H);
  auto px = solid(W, H, RED);

  YUV422P10Encoder enc;
  enc.init(rhi, *ctx.state, input, W, H);
  CHECK(enc.planeCount() == 3);
  runFragment(rhi, enc, input, px);

  const auto& Y = enc.readback(0);
  const auto& U = enc.readback(1);
  const auto& V = enc.readback(2);
  REQUIRE(Y.data.size() == W * H * 2);
  REQUIRE(U.data.size() == (W / 2) * H * 2);
  REQUIRE(V.data.size() == (W / 2) * H * 2);
  for(int i = 0; i < W * H; i++)
  {
    INFO("Y sample " << i << " = " << words16(Y)[i]);
    CHECK(words16(Y)[i] <= 1023); // low-bit placement
    CHECK(near(words16(Y)[i], Y_RED_10));
  }
  for(int i = 0; i < (W / 2) * H; i++)
  {
    INFO("chroma sample " << i);
    CHECK(near(words16(U)[i], U_RED_10));
    CHECK(near(words16(V)[i], V_RED_10));
  }
  enc.release();
  delete input;
}

TEST_CASE("YUVPlanarEncoder 8-bit 4:2:2 and 4:2:0", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  auto& rhi = *ctx.rhi;
  const int W = 8, H = 4;
  auto* input = makeInput(rhi, W, H);
  auto px = solid(W, H, RED);

  struct Case
  {
    std::unique_ptr<YUVPlanarEncoder> enc;
    int chromaH;
    const char* name;
  };
  Case cases[2];
  cases[0] = {YUVPlanarEncoder::p422_8(), H, "p422_8"};
  cases[1] = {YUVPlanarEncoder::p420_8(), H / 2, "p420_8"};

  for(auto& c : cases)
  {
    c.enc->init(rhi, *ctx.state, input, W, H);
    CHECK(c.enc->planeCount() == 3);
    runFragment(rhi, *c.enc, input, px);
    const auto& Y = c.enc->readback(0);
    const auto& Cb = c.enc->readback(1);
    const auto& Cr = c.enc->readback(2);
    INFO(c.name);
    REQUIRE(Y.data.size() == W * H);
    REQUIRE(Cb.data.size() == (W / 2) * c.chromaH);
    REQUIRE(Cr.data.size() == (W / 2) * c.chromaH);
    for(int i = 0; i < W * H; i++)
      CHECK(near(bytes(Y)[i], Y_RED_8));
    for(int i = 0; i < (W / 2) * c.chromaH; i++)
    {
      CHECK(near(bytes(Cb)[i], U_RED_8));
      CHECK(bytes(Cr)[i] == V_RED_8);
    }
    c.enc->release();
  }
  delete input;
}

TEST_CASE("YUVPlanarEncoder 10-bit 4:2:0 (R16 planes)", "[gfx][encoders][gpu]")
{
  REQUIRE_GPU_CTX();
  auto& rhi = *ctx.rhi;
  const int W = 8, H = 4;
  auto* input = makeInput(rhi, W, H);
  auto px = solid(W, H, RED);

  auto enc = YUVPlanarEncoder::p420_10();
  enc->init(rhi, *ctx.state, input, W, H);
  runFragment(rhi, *enc, input, px);

  const auto& Y = enc->readback(0);
  const auto& Cb = enc->readback(1);
  const auto& Cr = enc->readback(2);

  // Same *P10LE byte layout on both YUVPlanar 10-bit implementations
  // (Qt >= 6.10 native R16 targets, or the Qt < 6.10 RGBA8-packing fallback
  // added because the pre-6.10 GL readback expands R16 to 8-bit RGBA).
  REQUIRE(Y.data.size() == W * H * 2);
  REQUIRE(Cb.data.size() == (W / 2) * (H / 2) * 2);
  REQUIRE(Cr.data.size() == (W / 2) * (H / 2) * 2);
  for(int i = 0; i < W * H; i++)
  {
    INFO("Y sample " << i << " = " << words16(Y)[i]);
    CHECK(words16(Y)[i] <= 1023);
    CHECK(near(words16(Y)[i], Y_RED_10));
  }
  for(int i = 0; i < (W / 2) * (H / 2); i++)
  {
    INFO("chroma sample " << i);
    CHECK(near(words16(Cb)[i], U_RED_10));
    CHECK(near(words16(Cr)[i], V_RED_10));
  }
  enc->release();
  delete input;
}

// ===========================================================================
// Compute encoders (SSBO output)
// ===========================================================================
namespace
{
QRhiBuffer* makeSSBO(QRhi& rhi, int size)
{
  auto* buf = rhi.newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, size);
  if(!buf->create())
  {
    delete buf;
    return nullptr;
  }
  return buf;
}

#define REQUIRE_COMPUTE()                                                    \
  do                                                                         \
  {                                                                          \
    if(!ctx.rhi->isFeatureSupported(QRhi::Compute))                          \
      SKIP("QRhi backend has no compute support");                           \
    if(!ctx.rhi->isFeatureSupported(QRhi::ReadBackNonUniformBuffer))         \
      SKIP("QRhi backend cannot read back storage buffers");                 \
  } while(0)
}

TEST_CASE("BGRAComputeEncoder writes B,G,R,A words", "[gfx][encoders][gpu][compute]")
{
  REQUIRE_GPU_CTX();
  REQUIRE_COMPUTE();
  auto& rhi = *ctx.rhi;
  const int W = 8, H = 2;
  auto* input = makeInput(rhi, W, H);
  auto px = solid(W, H, Rgba{11, 22, 33, 200});
  auto* out = makeSSBO(rhi, W * H * 4);
  REQUIRE(out);

  BGRAComputeEncoder enc;
  // init() failure branch: null output buffer
  CHECK_FALSE(enc.init(rhi, *ctx.state, input, W, H, nullptr));
  REQUIRE(enc.init(rhi, *ctx.state, input, W, H, out));
  auto data = runCompute(rhi, enc, input, px, out, W * H * 4);
  REQUIRE(data.size() == W * H * 4);
  const auto* b = reinterpret_cast<const uint8_t*>(data.constData());
  for(int i = 0; i < W * H; i++)
  {
    INFO("pixel " << i);
    CHECK(b[i * 4 + 0] == 33);  // B
    CHECK(b[i * 4 + 1] == 22);  // G
    CHECK(b[i * 4 + 2] == 11);  // R
    CHECK(b[i * 4 + 3] == 200); // A
  }
  enc.release();
  delete out;
  delete input;
}

TEST_CASE("BGRAComputeEncoder vertical orientation", "[gfx][encoders][gpu][compute]")
{
  REQUIRE_GPU_CTX();
  REQUIRE_COMPUTE();
  auto& rhi = *ctx.rhi;
  const int W = 4, H = 2;
  auto* input = makeInput(rhi, W, H);
  auto px = rowPattern(W, {RED, BLUE});
  auto* out = makeSSBO(rhi, W * H * 4);
  REQUIRE(out);

  BGRAComputeEncoder enc;
  REQUIRE(enc.init(rhi, *ctx.state, input, W, H, out));
  auto data = runCompute(rhi, enc, input, px, out, W * H * 4);
  REQUIRE(data.size() == W * H * 4);
  const auto* b = reinterpret_cast<const uint8_t*>(data.constData());
  // Same flip convention as the fragment encoders: buffer row 0 is the
  // uploaded row H-1 (blue). Memory order is B,G,R,A.
  CHECK(b[0] == 255); // B of blue
  CHECK(b[2] == 0);   // R of blue
  const uint8_t* r1 = b + W * 4;
  CHECK(r1[0] == 0);   // B of red
  CHECK(r1[2] == 255); // R of red
  enc.release();
  delete out;
  delete input;
}

TEST_CASE("UYVYComputeEncoder packs Cb,Y0,Cr,Y1 words", "[gfx][encoders][gpu][compute]")
{
  REQUIRE_GPU_CTX();
  REQUIRE_COMPUTE();
  auto& rhi = *ctx.rhi;
  const int W = 8, H = 2;
  auto* input = makeInput(rhi, W, H);
  auto px = columns(W, H, BLACK, WHITE);
  const int size = W * 2 * H; // 2 bytes/px
  auto* out = makeSSBO(rhi, size);
  REQUIRE(out);

  UYVYComputeEncoder enc;
  // init() failure branches: null buffer, odd width
  CHECK_FALSE(enc.init(rhi, *ctx.state, input, W, H, nullptr));
  CHECK_FALSE(enc.init(rhi, *ctx.state, input, 7, H, out));
  REQUIRE(enc.init(rhi, *ctx.state, input, W, H, out));
  auto data = runCompute(rhi, enc, input, px, out, size);
  REQUIRE(data.size() == size);
  const auto* b = reinterpret_cast<const uint8_t*>(data.constData());
  for(int p = 0; p < W / 2 * H; p++)
  {
    INFO("pair " << p);
    CHECK(b[p * 4 + 0] == 128); // Cb (compute path: exact 128)
    CHECK(b[p * 4 + 1] == 0);   // Y0 black
    CHECK(b[p * 4 + 2] == 128); // Cr
    CHECK(b[p * 4 + 3] == 255); // Y1 white
  }
  enc.release();
  delete out;
  delete input;
}

TEST_CASE("V210ComputeEncoder packs the wire row", "[gfx][encoders][gpu][compute]")
{
  REQUIRE_GPU_CTX();
  REQUIRE_COMPUTE();
  auto& rhi = *ctx.rhi;
  const int W = 12, H = 2;
  auto* input = makeInput(rhi, W, H);
  auto px = solid(W, H, RED);
  const int stride = ((W + 47) / 48) * 128; // 128 bytes
  const int size = stride * H;
  auto* out = makeSSBO(rhi, size);
  REQUIRE(out);

  V210ComputeEncoder enc;
  // init() failure branches: null buffer, odd width (note: the compute
  // encoder itself only rejects odd widths; the %6 gate lives in
  // wireComputeSupports).
  CHECK_FALSE(enc.init(rhi, *ctx.state, input, W, H, nullptr));
  CHECK_FALSE(enc.init(rhi, *ctx.state, input, 11, H, out));
  REQUIRE(enc.init(rhi, *ctx.state, input, W, H, out));
  auto data = runCompute(rhi, enc, input, px, out, size);
  REQUIRE(data.size() == size);

  const int wordsPerRow = stride / 4;
  for(int row = 0; row < H; row++)
  {
    const size_t base = size_t(row) * wordsPerRow;
    for(int g = 0; g < 2; g++) // 12 px = 2 groups
    {
      checkV210Fields(
          word32(data, base + g * 4 + 0), U_RED_10, Y_RED_10, V_RED_10,
          "w0 = Cb|Y0|Cr");
      checkV210Fields(
          word32(data, base + g * 4 + 1), Y_RED_10, U_RED_10, Y_RED_10,
          "w1 = Y1|Cb|Y2");
      checkV210Fields(
          word32(data, base + g * 4 + 2), V_RED_10, Y_RED_10, U_RED_10,
          "w2 = Cr|Y3|Cb");
      checkV210Fields(
          word32(data, base + g * 4 + 3), Y_RED_10, V_RED_10, Y_RED_10,
          "w3 = Y4|Cr|Y5");
    }
  }
  enc.release();
  delete out;
  delete input;
}
