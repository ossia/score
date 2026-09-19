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
#include <Gfx/Graph/encoders/PackedRGB.hpp>
#include <Gfx/Graph/encoders/ColorSpaceOut.hpp>
#include <Gfx/Graph/encoders/WireEncoderFactory.hpp>
#include <Gfx/Graph/encoders/YUV422P10.hpp>

#include <algorithm>
#include <cmath>
#include <string>

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
  enc.init(rhi, state, input, w, h, score::gfx::colorMatrixOut());
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
  enc.init(rhi, state, input, w, h, score::gfx::colorMatrixOut());
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
    enc.init(rhi, state, input, w, h, score::gfx::colorMatrixOut());
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

// 10-bit packed RGB words. A colored constant input distinguishes the channel
// orders; the same encoder is then re-run with its readback disabled and the
// output texture read back manually, which is the contract the
// direct-readback output rung relies on (outputTexture + setReadbackEnabled).
void testPackedRGB(QRhi& rhi, const RenderState& state)
{
  std::printf("PackedRGBEncoder (10-bit packed words):\n");
  const int w = 64, h = 8;
  auto* input = rhi.newTexture(
      QRhiTexture::RGBA8, QSize(w, h), 1, QRhiTexture::UsedAsTransferSource);
  input->create();
  std::vector<uint8_t> px(size_t(w) * h * 4);
  for(size_t i = 0; i < px.size(); i += 4)
  {
    px[i] = 51;      // R -> 205 in 10 bits
    px[i + 1] = 102; // G -> 409
    px[i + 2] = 204; // B -> 818
    px[i + 3] = 255;
  }
  const uint32_t R = 205, G = 409, B = 818;

  struct Case
  {
    const char* name;
    std::unique_ptr<PackedRGBEncoder> enc;
    uint32_t word;
    bool bigEndian;
  };
  Case cases[] = {
      {"r210be (R<<20|G<<10|B, BE)", PackedRGBEncoder::r210be(),
       (R << 20) | (G << 10) | B, true},
      {"rgb10 (B<<20|G<<10|R, LE)", PackedRGBEncoder::rgb10(),
       (B << 20) | (G << 10) | R, false},
  };
  for(auto& c : cases)
  {
    uint8_t e[4];
    for(int k = 0; k < 4; ++k)
      e[k] = uint8_t((c.word >> (8 * (c.bigEndian ? 3 - k : k))) & 0xFF);

    c.enc->init(rhi, state, input, w, h, score::gfx::colorMatrixOut());
    uploadAndExec(rhi, *c.enc, input, px);
    const auto& rb = c.enc->readback(0);
    const auto* b = reinterpret_cast<const uint8_t*>(rb.data.constData());
    const bool ok = rb.data.size() >= 4 && b[0] == e[0] && b[1] == e[1]
                    && b[2] == e[2] && b[3] == e[3];
    char msg[96];
    std::snprintf(
        msg, sizeof msg, "%s bytes = %d,%d,%d,%d", c.name, e[0], e[1], e[2],
        e[3]);
    check(ok, msg);
    if(!ok && b && rb.data.size() >= 4)
      std::printf("    got %d,%d,%d,%d\n", b[0], b[1], b[2], b[3]);
    c.enc->release();
  }

  // Readback disabled: exec must leave the readback empty while the output
  // texture still carries the exact wire bytes.
  {
    auto enc = PackedRGBEncoder::r210be();
    enc->init(rhi, state, input, w, h, score::gfx::colorMatrixOut());
    enc->setReadbackEnabled(false);
    uploadAndExec(rhi, *enc, input, px);
    check(enc->readback(0).data.isEmpty(), "readback disabled -> no readback");

    QRhiReadbackResult trb;
    QRhiCommandBuffer* cb{};
    rhi.beginOffscreenFrame(&cb);
    auto* batch = rhi.nextResourceUpdateBatch();
    batch->readBackTexture(QRhiReadbackDescription{enc->outputTexture()}, &trb);
    cb->resourceUpdate(batch);
    rhi.endOffscreenFrame();
    const auto* b = reinterpret_cast<const uint8_t*>(trb.data.constData());
    const uint32_t word = (R << 20) | (G << 10) | B;
    bool ok = trb.data.size() >= 4;
    for(int k = 0; ok && k < 4; ++k)
      ok = b[k] == uint8_t((word >> (8 * (3 - k))) & 0xFF);
    check(ok, "output texture carries wire bytes with readback disabled");
    enc->release();
  }
  delete input;
}


// ---------------------------------------------------------------------------
// The contiguous-framestore encoders, against the plane-based ones they stand
// in for.
//
// makeWireEncoder(fmt, contiguousFramestore = true) returns an encoder whose
// SINGLE readback is the whole framestore, planes adjacent and in order,
// instead of one readback per plane. That exists so a consumer handing a
// device one pointer -- NDI's p_data, a capture card's frame buffer -- does
// not have to concatenate anything, and does not pay two or three separate
// GPU->CPU round trips to get there.
//
// The two routes must produce the SAME BYTES. Not nearly: a packed layout that
// is off by a row, or that assumes it may group bytes into RGBA texels, puts
// chroma somewhere the consumer will not look for it and produces a picture
// that is wrong while remaining structurally valid on the wire -- which is the
// kind of defect that ships.
//
// Widths matter as much as formats here. 722 and 1922 are even, so 4:2:0 can
// express them, but they are not multiples of four: that is where a packed
// layout built on RGBA texels breaks, and where an R8 readback would show row
// padding if it had any.
struct PlaneSpec
{
  int encoderPlane, widthDiv, heightDiv, bytesPerTexel;
};

struct FramestoreLayout
{
  const char* name;
  score::gfx::interop::VideoPixelFormat packedFmt;  // what to ask for, packed
  score::gfx::interop::VideoPixelFormat planeFmt;   // the plane-based twin
  int rowsNum, rowsDen;    // framestore rows = height * num / den
  int primaryBytesPerPixel;
  /// 1 for the 8-bit layouts, 2 for the 16-bit ones. A 16-bit format must be
  /// compared as SAMPLES: the packed encoder builds its two bytes arithmetically
  /// in the shader while the plane encoder lets an R16 UNORM target round, so
  /// the two can land 1 LSB apart -- and 1 LSB apart across a carry (0x1200 vs
  /// 0x11FF) is a 255 difference in the low BYTE. Byte-wise with zero tolerance
  /// would call that a layout bug.
  int bytesPerSample;
  int planeCount;
  PlaneSpec planes[3];
};

// Assemble plane readbacks into one tight framestore, exactly as a consumer
// would: each plane's own (possibly padded) stride in, tight rows out.
std::vector<uint8_t> assemble(
    GPUVideoEncoder& enc, const FramestoreLayout& L, int W, int H)
{
  const size_t rowBytes = size_t(W) * L.primaryBytesPerPixel;
  const size_t total = rowBytes * (size_t(H) * L.rowsNum / L.rowsDen);
  std::vector<uint8_t> out;
  out.reserve(total);

  for(int i = 0; i < L.planeCount; i++)
  {
    const auto& spec = L.planes[i];
    const auto& rb = enc.readback(spec.encoderPlane);
    const int rows = H / spec.heightDiv;
    const int tight = (W / spec.widthDiv) * spec.bytesPerTexel;
    if(rb.data.isEmpty() || rows <= 0 || tight <= 0)
      return {};
    const int srcStride = int(rb.data.size()) / rows;
    if(srcStride < tight)
      return {};
    const auto* p = reinterpret_cast<const uint8_t*>(rb.data.constData());
    for(int r = 0; r < rows; r++)
      out.insert(out.end(), p + size_t(r) * srcStride,
                 p + size_t(r) * srcStride + tight);
  }
  if(out.size() != total)
    return {};
  return out;
}


// Which side is wrong when packed and planes disagree?
//
// Decided without any colour arithmetic. The source is made to vary ONLY
// vertically, so every chroma row is a single repeated value by construction.
// A route whose rows are internally constant is reading its own bytes
// correctly; one that shows a transition part-way along a row is reading rows
// that are not where it thinks they are -- which is what a row-stride or
// alignment mismatch looks like from the outside.
void diagnosePackedRowStride(QRhi& rhi, const RenderState& state, int W, int H)
{
  std::printf(
      "\n  NV12 %dx%d: which route reads its own rows correctly?\n", W, H);

  std::vector<uint8_t> px(size_t(W) * H * 4);
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      uint8_t* q = px.data() + (size_t(y) * W + x) * 4;
      q[0] = uint8_t((y * 5) & 0xFF);      // varies with y only
      q[1] = uint8_t(255 - ((y * 3) & 0xFF));
      q[2] = uint8_t((y * 11) & 0xFF);
      q[3] = 255;
    }

  auto* input = rhi.newTexture(
      QRhiTexture::RGBA8, QSize(W, H), 1, QRhiTexture::UsedAsTransferSource);
  input->create();
  const QString matrix = colorMatrixOut(
      AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_MPEG, AVCOL_PRI_BT709);

  auto packed = score::gfx::makeWireEncoder(
      score::gfx::interop::VideoPixelFormat::NV12, true);
  auto planar = score::gfx::makeWireEncoder(
      score::gfx::interop::VideoPixelFormat::NV12, false);
  packed->init(rhi, state, input, W, H, matrix);
  planar->init(rhi, state, input, W, H, matrix);
  packed->setReadbackEnabled(true);
  planar->setReadbackEnabled(true);
  uploadAndExec(rhi, *packed, input, px);
  uploadAndExec(rhi, *planar, input, px);

  // Each chroma row holds W bytes: W/2 sites of (Cb, Cr). Constant source
  // along x means every Cb in a row is equal, and every Cr likewise.
  auto rowIsConstant = [W](const uint8_t* row) {
    for(int i = 2; i + 1 < W; i += 2)
      if(row[i] != row[0] || row[i + 1] != row[1])
        return false;
    return true;
  };

  const auto& prb = packed->readback(0);
  const auto* pdata = reinterpret_cast<const uint8_t*>(prb.data.constData());
  const uint8_t* packedChroma = pdata + size_t(W) * H;

  const auto& crb = planar->readback(1);
  const auto* planeChroma = reinterpret_cast<const uint8_t*>(crb.data.constData());

  int packedBad = 0, planeBad = 0, firstPackedBad = -1, firstPlaneBad = -1;
  for(int r = 0; r < H / 2; r++)
  {
    if(!rowIsConstant(packedChroma + size_t(r) * W))
    {
      if(firstPackedBad < 0) firstPackedBad = r;
      ++packedBad;
    }
    if(!rowIsConstant(planeChroma + size_t(r) * W))
    {
      if(firstPlaneBad < 0) firstPlaneBad = r;
      ++planeBad;
    }
  }

  std::printf(
      "    packed : %d of %d chroma rows are NOT constant (first %d)\n",
      packedBad, H / 2, firstPackedBad);
  std::printf(
      "    planes : %d of %d chroma rows are NOT constant (first %d)\n",
      planeBad, H / 2, firstPlaneBad);
  std::printf(
      "    plane-1 readback %d bytes; tight would be %d (%d x %d)\n",
      int(crb.data.size()), W * (H / 2), W, H / 2);

  // The invariant that does not depend on the plane route at all: a source
  // constant along x must produce chroma rows constant along x. This is what
  // makes "packed is the correct one" a measurement rather than an assumption.
  char what[160];
  std::snprintf(
      what, sizeof what,
      "NV12 %dx%d packed: every chroma row is constant for an x-constant source",
      W, H);
  check(packedBad == 0, what);

  packed->release();
  planar->release();
  delete input;
}


// The same x-constant invariant, for P216's 16-bit luma. Each luma sample is
// two little-endian bytes, so an x-constant source must make every luma row
// repeat with period 2. Independent of the plane route, like the NV12 one.
void diagnoseP216RowStride(QRhi& rhi, const RenderState& state, int W, int H)
{
  std::printf("\n  P216 %dx%d: which route reads its own luma rows correctly?\n", W, H);
  std::vector<uint8_t> px(size_t(W) * H * 4);
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      uint8_t* q = px.data() + (size_t(y) * W + x) * 4;
      q[0] = uint8_t((y * 5) & 0xFF);
      q[1] = uint8_t(255 - ((y * 3) & 0xFF));
      q[2] = uint8_t((y * 11) & 0xFF);
      q[3] = 255;
    }

  auto* input = rhi.newTexture(
      QRhiTexture::RGBA8, QSize(W, H), 1, QRhiTexture::UsedAsTransferSource);
  input->create();
  const QString matrix = colorMatrixOut(
      AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_MPEG, AVCOL_PRI_BT709);
  auto packed = score::gfx::makeWireEncoder(
      score::gfx::interop::VideoPixelFormat::P216, true);
  auto planar = score::gfx::makeWireEncoder(
      score::gfx::interop::VideoPixelFormat::P216, false);
  packed->init(rhi, state, input, W, H, matrix);
  planar->init(rhi, state, input, W, H, matrix);
  packed->setReadbackEnabled(true);
  planar->setReadbackEnabled(true);
  uploadAndExec(rhi, *packed, input, px);
  uploadAndExec(rhi, *planar, input, px);

  auto rowPeriod2 = [W](const uint8_t* row) {
    for(int i = 2; i + 1 < W * 2; i += 2)
      if(row[i] != row[0] || row[i + 1] != row[1])
        return false;
    return true;
  };

  const auto* p = reinterpret_cast<const uint8_t*>(packed->readback(0).data.constData());
  const auto* q = reinterpret_cast<const uint8_t*>(planar->readback(0).data.constData());
  int pb = 0, qb = 0, fp = -1, fq = -1;
  for(int r = 0; r < H; r++)
  {
    if(!rowPeriod2(p + size_t(r) * W * 2)) { if(fp < 0) fp = r; ++pb; }
    if(!rowPeriod2(q + size_t(r) * W * 2)) { if(fq < 0) fq = r; ++qb; }
  }
  std::printf("    packed : %d of %d luma rows are NOT constant (first %d)\n", pb, H, fp);
  std::printf("    planes : %d of %d luma rows are NOT constant (first %d)\n", qb, H, fq);
  char what[160];
  std::snprintf(
      what, sizeof what,
      "P216 %dx%d packed: every luma row is constant for an x-constant source", W, H);
  check(pb == 0, what);
  packed->release(); planar->release(); delete input;
}

void testContiguousFramestore(QRhi& rhi, const RenderState& state)
{
  using F = score::gfx::interop::VideoPixelFormat;
  std::printf("\ncontiguous framestore == planes, byte for byte:\n");

  const FramestoreLayout layouts[] = {
      {"NV12", F::NV12, F::NV12, 3, 2, 1, 1, 2, {{0, 1, 1, 1}, {1, 2, 2, 2}}},
      {"I420", F::YUV420P, F::YUV420P, 3, 2, 1, 1, 3,
       {{0, 1, 1, 1}, {1, 2, 2, 1}, {2, 2, 2, 1}}},
      // YV12 has no plane-based encoder of its own: with planes, which of
      // Cb/Cr comes first is the consumer's business. Its reference is
      // therefore YUV420P's planes taken in the swapped order.
      {"YV12", F::YVU420P, F::YUV420P, 3, 2, 1, 1, 3,
       {{0, 1, 1, 1}, {2, 2, 2, 1}, {1, 2, 2, 1}}},
      // 16-bit 4:2:2. Its two planes are the same size, so the framestore is
      // one stacked on the other -- the easy case, and the one that shipped
      // first. It is in this table because it did NOT always pass: the packed
      // encoder was picking the wrong source pixel pair for under 1% of
      // samples, which no flat-colour test and no round trip through the SDK
      // could see. ffmpeg found it (see P216Packed.hpp).
      {"P216", F::P216, F::P216, 2, 1, 2, 2, 2, {{0, 1, 1, 2}, {1, 1, 1, 2}}},
  };

  struct Size { int w, h; const char* label; };
  const Size sizes[] = {
      {64, 48, "small"},
      {320, 240, "QVGA"},
      {720, 576, "PAL D1"},
      {722, 576, "722 - even, NOT a multiple of 4"},
      {1280, 720, "720p"},
      {1920, 1080, "1080p"},
      {1922, 1080, "1922 - even, NOT a multiple of 4"},
  };

  for(const auto& L : layouts)
  {
    for(const auto& sz : sizes)
    {
      const int W = sz.w, H = sz.h;
      std::vector<uint8_t> px(size_t(W) * H * 4);
      // A pattern with real chroma detail in both axes: a flat field would
      // hide every subsampling and plane-order mistake there is.
      for(int y = 0; y < H; y++)
        for(int x = 0; x < W; x++)
        {
          uint8_t* q = px.data() + (size_t(y) * W + x) * 4;
          q[0] = uint8_t((x * 7 + y * 3) & 0xFF);
          q[1] = uint8_t((x * 3 + y * 11) & 0xFF);
          q[2] = uint8_t((x * 13 + y * 5) & 0xFF);
          q[3] = 255;
        }

      auto* input = rhi.newTexture(
          QRhiTexture::RGBA8, QSize(W, H), 1, QRhiTexture::UsedAsTransferSource);
      input->create();

      auto packed = score::gfx::makeWireEncoder(L.packedFmt, true);
      auto planar = score::gfx::makeWireEncoder(L.planeFmt, false);
      if(!packed || !planar)
      {
        check(false, "both routes exist");
        delete input;
        continue;
      }

      // A real conversion shader, not an empty string: it is what defines
      // convert_from_rgb, and both sides must get the SAME one or the
      // comparison measures the matrix instead of the layout.
      const QString matrix = colorMatrixOut(
          AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_MPEG, AVCOL_PRI_BT709);
      packed->init(rhi, state, input, W, H, matrix);
      planar->init(rhi, state, input, W, H, matrix);
      packed->setReadbackEnabled(true);
      planar->setReadbackEnabled(true);

      uploadAndExec(rhi, *packed, input, px);
      uploadAndExec(rhi, *planar, input, px);

      const size_t rowBytes = size_t(W) * L.primaryBytesPerPixel;
      const size_t rows = size_t(H) * L.rowsNum / L.rowsDen;
      const size_t total = rowBytes * rows;

      const auto& prb = packed->readback(0);
      const auto* pdata = reinterpret_cast<const uint8_t*>(prb.data.constData());
      const auto ref = assemble(*planar, L, W, H);

      char what[192];
      if(ref.empty() || prb.data.isEmpty())
      {
        std::snprintf(what, sizeof what, "%s %dx%d: both routes produced bytes",
                      L.name, W, H);
        check(false, what);
        packed->release(); planar->release(); delete input;
        continue;
      }

      // A padded packed readback would shift the whole layout: byte n would no
      // longer be framestore byte n.
      std::snprintf(what, sizeof what,
                    "%s %dx%d (%s): packed readback is tight, %zu bytes",
                    L.name, W, H, sz.label, total);
      check(size_t(prb.data.size()) == total, what);

      size_t nDiff = 0, firstDiff = total;
      int worst = 0;
      const size_t n = std::min(size_t(prb.data.size()), ref.size());
      if(L.bytesPerSample == 2)
      {
        // Compare 16-bit samples, tolerating the 1 LSB the two rounding paths
        // can differ by. Anything structural is orders of magnitude bigger:
        // the indexing bug this test caught in P216PackedEncoder showed up as
        // differences of tens of thousands.
        for(size_t i = 0; i + 1 < n; i += 2)
        {
          const int a = pdata[i] | (pdata[i + 1] << 8);
          const int b = ref[i] | (ref[i + 1] << 8);
          const int d = std::abs(a - b);
          if(d > 1)
          {
            if(firstDiff == total)
              firstDiff = i;
            ++nDiff;
            worst = std::max(worst, d);
          }
        }
      }
      else
      {
        for(size_t i = 0; i < n; i++)
        {
          const int d = std::abs(int(pdata[i]) - int(ref[i]));
          if(d != 0)
          {
            if(firstDiff == total)
              firstDiff = i;
            ++nDiff;
            worst = std::max(worst, d);
          }
        }
      }

      // No exemption by width any more. The plane route used to be an
      // unusable reference at 722 and 1922 -- NV12's RG8 chroma target read
      // its rows back shifted there -- so this check was skipped at those
      // sizes and the packed route was held to its own invariant instead.
      // NV12Encoder renders its chroma into R8 now and is readable at every
      // width, so the two routes are compared everywhere, which is the check
      // worth having.
      std::snprintf(
          what, sizeof what, "%s %dx%d (%s): packed == planes",
          L.name, W, H, sz.label);
      check(nDiff == 0 && n == total, what);

      if(nDiff != 0)
      {
        // Say WHERE, so the next reader does not have to bisect it by hand.
        const size_t lumaBytes = rowBytes * H;
        std::printf(
            "        %zu bytes differ (worst %d); first at %zu, which is in the "
            "%s (luma ends at %zu, row %zu col %zu)\n",
            nDiff, worst, firstDiff, firstDiff < lumaBytes ? "LUMA" : "CHROMA",
            lumaBytes, firstDiff / rowBytes, firstDiff % rowBytes);
        for(int i = 0; i < L.planeCount; i++)
          std::printf(
              "        plane %d readback: %d bytes\n", L.planes[i].encoderPlane,
              int(planar->readback(L.planes[i].encoderPlane).data.size()));
      }

      packed->release();
      planar->release();
      delete input;
    }
  }
}

void runTests()
{
  const int W = 192, H = 64; // W % 48 == 0, even H
  // Backend selectable via SCORE_TEST_API=vulkan|opengl|d3d11|d3d12
  // (default OpenGL).
  const QByteArray apiEnv = qgetenv("SCORE_TEST_API").toLower();
  const GraphicsApi api = (apiEnv == "vulkan" || apiEnv == "vk")
                              ? GraphicsApi::Vulkan
                          : apiEnv == "d3d11" ? GraphicsApi::D3D11
                          : apiEnv == "d3d12" ? GraphicsApi::D3D12
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
  testPackedRGB(rhi, *state);
  testContiguousFramestore(rhi, *state);
  diagnosePackedRowStride(rhi, *state, 722, 576);
  diagnosePackedRowStride(rhi, *state, 1922, 1080);
  diagnoseP216RowStride(rhi, *state, 1920, 1080);

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
        #if defined(SCORE_TEST_HAS_LSAN)
        // _Exit skips LSan's atexit hook; run the leak check explicitly so
        // sanitizer builds still report leaks (dies non-zero on findings).
        __lsan_do_leak_check();
#endif
        std::_Exit(g_fail ? 1 : 0); // skip Qt/score teardown (segfaults)
      },
      Qt::QueuedConnection);
  return app.exec();
}
