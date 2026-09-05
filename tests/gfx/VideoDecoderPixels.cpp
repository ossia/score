// =============================================================================
// L3 -- what the packed / high-bit-depth GPU video decoders actually PUT ON
// SCREEN.
//
// tests/unit/VideoDecoderFactoryTest.cpp proves which decoder each
// AVPixelFormat resolves to. tests/integration/video-decode-correctness.sh
// proves real pixels, but only for what ffmpeg can round-trip through a
// container that preserves the pix_fmt -- which excludes every packed format
// here (no muxer stores y210/xv30/vuya, and yuva444p12 does not survive one).
// So these four decoders were selectable and nothing more.
//
// The frames are therefore synthesised: a std::vector of bytes laid out by hand
// per the format's specification, handed to a fake Video::ExternalInput, and
// pushed through a real score::gfx::CameraNode -> VideoNodeRenderer ->
// createGPUVideoDecoder -> offscreen readback. Nothing about the render path is
// faked; only the frame source is.
//
// WHERE THE EXPECTATIONS COME FROM. Not from score. Each colour is a
// (Y, Cb, Cr) triple whose RGB is computed from ITU-R BT.709 full range,
//   R = Y + 1.5748·Cr'   B = Y + 1.8556·Cb'
//   G = Y - 0.468124·Cr' - 0.187324·Cb'
// with Y, Cb, Cr the 8-bit codes over 255 and Cb' = (Cb-128)/255,
// Cr' = (Cr-128)/255 -- H.273 full range puts neutral chroma at code 128
// and scales by 255, so the centre is 128/255, not 0.5 (a 0.5 centre puts
// `cr-hi` green at 68; ffmpeg says 69).
// BOTH the byte layouts and those RGB values were then cross-checked, before
// this file was written, by feeding the exact same synthesised bytes to
// `ffmpeg -f rawvideo -pix_fmt <fmt> ... -vf format=rgb24` and reading the
// result. ffmpeg's swscale and this file agree on every cell of the table below;

// score is the only party being tested.
//
// Full range and an explicit AVCOL_SPC_BT709 are used on purpose: they pin one
// unambiguous matrix out of the fourteen colorMatrix() can emit, and the
// limited-range family is already exercised by video-decode-correctness.sh. The
// two chroma-extreme colours are the ones that matter: `cr-hi` and `cb-hi`
// differ in EVERY channel, so a Cb/Cr swap, an R/B swap, a limited/full range
// mistake and a BT.601/BT.709 mistake each move the readback well outside the
// tolerance. A uniform grey, which is what a "not blank" check would accept,
// survives all four.
//
// The luma-ramp case covers the other half: a per-pixel unpack error (reading
// the wrong 10-bit field, the wrong half of a macropixel, the wrong stride)
// leaves the colour cases correct and only shows up as a wrong value at a known
// x.
//
// WHAT THIS FOUND. The RGBA8 control decodes correctly on both backends. Four
// defects:
//
//  1. Every full-range matrix in ColorSpace.hpp centred chroma on 0.5 instead of
//     128/255, so R and B came back one code high at EVERY luma level, black
//     included -- (1,0,1) for black, (129,128,129) for grey -- on 8-bit VUYA
//     just as much as on 10-bit Y210. G moved by only -0.33 of a code and so
//     stayed put. FIXED.
//
//  2. The high-bit-depth scale factors were all 0.39% low: P010 / P210 / P410 /
//     P016 / RGB48 / RGBA64 / GRAY16 / YA16 / GBRP16 applied none at all, the
//     LSB-aligned planar decoders used 64 and 16 where the format needs
//     65535/1020 and 65535/4080, and XV30 took RGB10A2's code/1023 as final.
//     FIXED; this is what kTol = 0 costs.
//
//  3. Y210Decoder allocated an RGBA16F texture and uploaded UNORM16 samples
//     into it, so every sample was reinterpreted as a half-float and the
//     picture collapsed to two constants. Same defect class as the
//     rgba64le/bgra64le one video-decode-correctness.sh records as fixed. FIXED
//     -- the decoder now uploads RG16 unorm at {w, h}; both Y210 cases below
//     assert the correct pixels and are no longer [!shouldfail].
//
//  4. VUYADecoder and XV30Decoder are gated in GPUVideoDecoderFactory.cpp on a
//     libavutil far later than the one that introduced their pixel formats, so
//     on every shipping ffmpeg 6.x/7.x they are unreachable dead code and those
//     formats render black. Still open; carries a [!shouldfail] case with the
//     correct expectation and the evidence.
//
// NOT COVERED HERE, and why:
//  - V210 / R210 / Bayer / PackedBitfield* are wire decoders: makeWireDecoder()
//    keys on score::gfx::interop::VideoPixelFormat, reached only from a capture
//    backend in score-addon-aja, never from an AVPixelFormat. There is no
//    AVFrame that selects them, so this fixture cannot reach them at all.
//  - The HW families (VAAPI / CUDA / Vulkan / D3D11 / D3D12 / VideoToolbox /
//    DRM-PRIME / HWTransfer) need an AVFrame carrying a real hw_frames_ctx and
//    a live device. See the "unhandled and hardware formats" case, which covers
//    the one thing that IS reachable: that an unusable format degrades to the
//    EmptyDecoder instead of crashing the renderer.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/VideoNode.hpp>

#include <Video/ExternalInput.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
}

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Two different questions, and for VUYA / VUYX / XV30 they have different
// answers on this host -- which is itself a finding, see the *_NAMEABLE cases.
//
//  NAMEABLE   : this libavutil declares the AVPixelFormat, so a frame can carry
//               it and a test can build one. Verified present in libavutil
//               58.29.100 (ffmpeg 6.1), which is what the bound below records.
//  SELECTABLE : GPUVideoDecoderFactory.cpp will actually hand back the decoder.
//               Its guards are reproduced verbatim here.
#define SCORE_TEST_HAS_Y210 (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100))

#define SCORE_TEST_VUYA_NAMEABLE (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100))
#define SCORE_TEST_VUYA_SELECTABLE (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100))
#define SCORE_TEST_XV30_NAMEABLE SCORE_TEST_VUYA_NAMEABLE
#define SCORE_TEST_XV30_SELECTABLE \
  (SCORE_TEST_VUYA_SELECTABLE && QT_VERSION >= QT_VERSION_CHECK(6, 4, 0))

using namespace score::test::gfx;

namespace
{
constexpr int W = 64;
constexpr int H = 64;

// An 8-bit (Y, Cb, Cr) triple and the RGB every format below must produce from
// it. Scaled up by 4 / 16 for the 10- and 12-bit formats.
struct Color
{
  const char* name;
  int y, cb, cr;
  int r, g, b;
};

// Cross-checked against ffmpeg swscale (see the header comment). `cr-hi` clips
// red, which is intentional: the clamp is part of the contract for an RGBA8
// target and a decoder that wrapped instead of clamping would read far too low.
constexpr Color kColors[]{
    {"grey", 128, 128, 128, 128, 128, 128},
    {"white", 255, 128, 128, 255, 255, 255},
    {"black", 0, 128, 128, 0, 0, 0},
    {"cr-hi", 128, 128, 255, 255, 69, 128},
    {"cb-hi", 128, 255, 128, 128, 104, 255},
};

// Frame bytes, in the layout the AVPixelFormat prescribes.
struct Planes
{
  std::vector<uint8_t> data[4];
  int linesize[4]{};
  int count = 1;
};

void put16le(std::vector<uint8_t>& v, uint16_t x)
{
  v.push_back(uint8_t(x & 0xFF));
  v.push_back(uint8_t(x >> 8));
}
void put32le(std::vector<uint8_t>& v, uint32_t x)
{
  for(int i = 0; i < 4; i++)
    v.push_back(uint8_t((x >> (8 * i)) & 0xFF));
}

// --- packers. `yAt(x)` lets a case vary luma per column. ---------------------

// RGBA8, the control: no unpack, no matrix. If this one is wrong the fixture is.
Planes packRgba(int r, int g, int b)
{
  Planes p;
  p.count = 1;
  p.linesize[0] = W * 4;
  for(int i = 0; i < W * H; i++)
  {
    p.data[0].push_back(uint8_t(r));
    p.data[0].push_back(uint8_t(g));
    p.data[0].push_back(uint8_t(b));
    p.data[0].push_back(255);
  }
  return p;
}

// Y210: packed 4:2:2, each component a 16-bit LE word carrying its 10 bits in
// the HIGH end (value << 6). One macropixel per two columns: Y0 Cb Y1 Cr.
template <typename F>
Planes packY210(F yAt, int cb, int cr)
{
  Planes p;
  p.count = 1;
  p.linesize[0] = W * 4;
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x += 2)
    {
      put16le(p.data[0], uint16_t((yAt(x) * 4) << 6));
      put16le(p.data[0], uint16_t((cb * 4) << 6));
      put16le(p.data[0], uint16_t((yAt(x + 1) * 4) << 6));
      put16le(p.data[0], uint16_t((cr * 4) << 6));
    }
  return p;
}

// XV30 (Y410 layout): one 32-bit LE word per pixel,
// bits 0-9 = Cb, 10-19 = Y, 20-29 = Cr, 30-31 = alpha.
template <typename F>
Planes packXv30(F yAt, int cb, int cr)
{
  Planes p;
  p.count = 1;
  p.linesize[0] = W * 4;
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
      put32le(
          p.data[0], uint32_t(cb * 4) | (uint32_t(yAt(x) * 4) << 10)
                         | (uint32_t(cr * 4) << 20) | (3u << 30));
  return p;
}

// VUYA / VUYX: four bytes per pixel in the order V, U, Y, A.
template <typename F>
Planes packVuya(F yAt, int cb, int cr)
{
  Planes p;
  p.count = 1;
  p.linesize[0] = W * 4;
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      p.data[0].push_back(uint8_t(cr));
      p.data[0].push_back(uint8_t(cb));
      p.data[0].push_back(uint8_t(yAt(x)));
      p.data[0].push_back(255);
    }
  return p;
}

// P010 / P210 / P410 / P016: a full-resolution 16-bit luma plane and an
// interleaved Cb,Cr plane, subsampled by (sx, sy). Every sample is the 8-bit
// code left-aligned in the word -- code << (16 - bits) -- which is exactly what
// SMPTE 274M prescribes and what ffmpeg reads back with `>> (bits - 8)`.
template <typename F>
Planes packSemiPlanarMsb(F yAt, int cb, int cr, int bits, int sx, int sy)
{
  const int shift = 16 - bits;
  const int scale = 1 << (bits - 8);
  Planes p;
  p.count = 2;
  p.linesize[0] = W * 2;
  p.linesize[1] = (W / sx) * 4;
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
      put16le(p.data[0], uint16_t((yAt(x) * scale) << shift));
  for(int y = 0; y < H / sy; y++)
    for(int x = 0; x < W / sx; x++)
    {
      put16le(p.data[1], uint16_t((cb * scale) << shift));
      put16le(p.data[1], uint16_t((cr * scale) << shift));
    }
  return p;
}

// RGBA64LE / GRAY16LE: no matrix at all, just a 16-bit word per component.
// Same framing rule -- 255 is 65280, not 65535.
Planes packRgba64(int r, int g, int b)
{
  Planes p;
  p.count = 1;
  p.linesize[0] = W * 8;
  for(int i = 0; i < W * H; i++)
  {
    put16le(p.data[0], uint16_t(r << 8));
    put16le(p.data[0], uint16_t(g << 8));
    put16le(p.data[0], uint16_t(b << 8));
    put16le(p.data[0], uint16_t(255 << 8));
  }
  return p;
}

template <typename F>
Planes packGray16(F yAt)
{
  Planes p;
  p.count = 1;
  p.linesize[0] = W * 2;
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
      put16le(p.data[0], uint16_t(yAt(x) << 8));
  return p;
}

// YUVA444P12LE: four full-resolution planes of 12-bit LE samples.
template <typename F>
Planes packYuva444p12(F yAt, int cb, int cr)
{
  Planes p;
  p.count = 4;
  for(int i = 0; i < 4; i++)
    p.linesize[i] = W * 2;
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      put16le(p.data[0], uint16_t(yAt(x) * 16));
      put16le(p.data[1], uint16_t(cb * 16));
      put16le(p.data[2], uint16_t(cr * 16));
      put16le(p.data[3], uint16_t(4095));
    }
  return p;
}

// A frame source that hands out the bytes above forever. The AVFrames it
// produces own nothing: data[] points into `planes`, which outlives them.
struct FakeCamera final : Video::ExternalInput
{
  Planes planes;

  FakeCamera(AVPixelFormat fmt, Planes pl, AVColorSpace spc, AVColorRange rng)
      : planes{std::move(pl)}
  {
    this->width = W;
    this->height = H;
    this->pixel_format = fmt;
    this->color_space = spc;
    this->color_range = rng;
    this->color_primaries = AVCOL_PRI_BT709;
    this->color_trc = AVCOL_TRC_BT709;
    this->fps = 25.;
    this->realTime = true;
  }

  bool start() noexcept override { return true; }
  void stop() noexcept override { }

  AVFrame* dequeue_frame() noexcept override
  {
    auto* f = av_frame_alloc();
    if(!f)
      return nullptr;
    f->format = pixel_format;
    f->width = width;
    f->height = height;
    f->color_range = color_range;
    f->color_primaries = color_primaries;
    f->color_trc = color_trc;
    f->colorspace = color_space;
    for(int i = 0; i < planes.count; i++)
    {
      f->data[i] = planes.data[i].empty() ? nullptr : planes.data[i].data();
      f->linesize[i] = planes.linesize[i];
    }
    return f;
  }

  // No buf[] was ever attached, so this frees the carrier and leaves the
  // pixels (owned by `planes`) alone.
  void release_frame(AVFrame* frame) noexcept override { av_frame_free(&frame); }
};

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  ReadbackImage img;
};

Outcome render_camera(
    score::gfx::GraphicsApi api, AVPixelFormat fmt, Planes planes,
    AVColorSpace spc = AVCOL_SPC_BT709, AVColorRange rng = AVCOL_RANGE_JPEG)
{
  Outcome out;
  out.backend = backend_name(api);

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto cam = std::make_shared<FakeCamera>(fmt, std::move(planes), spc, rng);
    const int cameraIdx
        = p.addNode(std::make_unique<score::gfx::CameraNode>(cam, QString{}));
    if(cameraIdx < 0)
    {
      out.error = p.error();
      return;
    }
    const int sink = p.addSink({W, H});
    p.wire(p.nodeImageOut(cameraIdx, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    // Four frames: the first pump is what gives the node a dequeued frame at
    // all, and the pipeline is warm well before the readback.
    p.render(4);
    out.img = p.readback(sink);
  });
  return out;
}

// Zero. Every colour below is exact arithmetic on 8-bit codes, every decoder
// here normalises an n-bit code to code / (255 * 2^(n-8)) so the 8-bit code
// comes back unchanged, and the RGBA8 target rounds to nearest. Nothing in the
// chain is allowed to move a pixel by one.
constexpr int kTol = 0;

void check_color(const ReadbackImage& img, const Color& c, const char* what)
{
  REQUIRE(img.valid());
  const auto px = img.at(W / 2, H / 2);
  INFO(
      what << " " << c.name << ": got (" << int(px[0]) << "," << int(px[1]) << ","
           << int(px[2]) << ") want (" << c.r << "," << c.g << "," << c.b << ")");
  CHECK(std::abs(int(px[0]) - c.r) <= kTol);
  CHECK(std::abs(int(px[1]) - c.g) <= kTol);
  CHECK(std::abs(int(px[2]) - c.b) <= kTol);
}
}

TEST_CASE(
    "RGBA control: the camera fixture reproduces pixels exactly",
    "[gfx][video][decoder][pixels]")
{
  // Everything below reads the same readback path. If this case is wrong,
  // nothing else in the file means anything -- so it is asserted first, and
  // tightly: an RGBA8 frame through a passthrough matrix must survive intact.
  // A sRGB-flagged target or an EOTF anywhere in the chain would move 64 to 137.
  const auto api = GENERATE(from_range(platform_backends()));
  const auto out = render_camera(
      api, AV_PIX_FMT_RGBA, packRgba(64, 160, 200), AVCOL_SPC_RGB,
      AVCOL_RANGE_JPEG);

  INFO("backend " << out.backend);
  if(out.skipped)
    SKIP(out.skip_reason);
  REQUIRE(out.error.empty());
  REQUIRE(out.img.valid());

  const auto px = out.img.at(W / 2, H / 2);
  INFO("got (" << int(px[0]) << "," << int(px[1]) << "," << int(px[2]) << ")");
  CHECK(std::abs(int(px[0]) - 64) <= 1);
  CHECK(std::abs(int(px[1]) - 160) <= 1);
  CHECK(std::abs(int(px[2]) - 200) <= 1);
}

#if SCORE_TEST_HAS_Y210
TEST_CASE(
    "Y210Decoder unpacks 10-bit 4:2:2 to the right colour",
    "[gfx][video][decoder][pixels]")
{
  // WAS [!shouldfail]. Y210Decoder::init() allocated QRhiTexture::RGBA16F and
  // exec() uploaded the raw y210 bytes into it, so the sampler reinterpreted
  // UNORM16 sample data as IEEE half-floats: grey (0x8000 = -0.0) came back
  // (0, 84, 0) and white (0xFFC0 = NaN) came back black, on both backends.
  // Same defect that video-decode-correctness.sh records as fixed for
  // rgba64le/bgra64le; y210 kept it because no container stores y210, so that
  // harness could never reach it.
  //
  // The decoder now uploads into an RG16 unorm texture at {w, h} -- 4 bytes per
  // texel is exactly one luma plus one chroma sample of the macropixel -- and
  // reassembles the triple with texelFetch.
  //
  // The readback also ran 0.39% low (white 254 for 255) until the sampler's
  // v/65535 was corrected by 65535/65280 -- see SCORE_GFX_MSB_ALIGNED_SCALE.
  const auto api = GENERATE(from_range(platform_backends()));
  for(const auto& c : kColors)
  {
    const auto out = render_camera(
        api, AV_PIX_FMT_Y210LE, packY210([&](int) { return c.y; }, c.cb, c.cr));
    INFO("backend " << out.backend);
    if(out.skipped)
      SKIP(out.skip_reason);
    REQUIRE(out.error.empty());
    check_color(out.img, c, "y210");
  }
}

#endif

#if SCORE_TEST_XV30_SELECTABLE
TEST_CASE("XV30Decoder unpacks 10-bit 4:4:4 to the right colour", "[gfx][video][decoder][pixels]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  for(const auto& c : kColors)
  {
    const auto out = render_camera(
        api, AV_PIX_FMT_XV30LE, packXv30([&](int) { return c.y; }, c.cb, c.cr));
    INFO("backend " << out.backend);
    if(out.skipped)
      SKIP(out.skip_reason);
    REQUIRE(out.error.empty());
    check_color(out.img, c, "xv30");
  }
}

#endif

#if SCORE_TEST_VUYA_SELECTABLE
TEST_CASE("VUYADecoder unpacks V,U,Y,A byte order", "[gfx][video][decoder][pixels]")
{
  // VUYA and VUYX differ only in whether alpha is honoured; both must read the
  // SAME three colour bytes out of the same positions, which is what a
  // component-order mistake would break.
  const auto api = GENERATE(from_range(platform_backends()));
  const auto fmt = GENERATE(AV_PIX_FMT_VUYA, AV_PIX_FMT_VUYX);
  for(const auto& c : kColors)
  {
    const auto out
        = render_camera(api, fmt, packVuya([&](int) { return c.y; }, c.cb, c.cr));
    INFO("backend " << out.backend);
    if(out.skipped)
      SKIP(out.skip_reason);
    REQUIRE(out.error.empty());
    check_color(out.img, c, av_get_pix_fmt_name(fmt));
  }
}

#endif

// NO PIN HERE ANY MORE, and the reason is worth recording. This file used to
// carry a [!shouldfail] case, "VUYA / VUYX / XV30 reach a decoder at all",
// compiled in under `NAMEABLE && !SELECTABLE`. Its finding: libavutil declares
// those three formats from 58.29.100 (ffmpeg 6.1), but GPUVideoDecoderFactory
// gated all three behind AV_VERSION_INT(60, 8, 100) -- ffmpeg 8 -- so on every
// shipping 6.x and 7.x build VUYADecoder and XV30Decoder were dead code, the
// switch fell through to EmptyDecoder, and the frame rendered black.
//
// The gate was lowered to 58.29.100 (GPUVideoDecoderFactory.cpp:251) and the
// finding is closed. SELECTABLE reproduces the product guard verbatim, so it
// now equals NAMEABLE and the pin condition became unsatisfiable on every
// ffmpeg -- an unreachable TEST_CASE, which is not a pin at all. Deleted rather
// than left to rot: an inventory that lists a case which can never run lies
// about what is covered.
//
// Nothing replaces it, and nothing needs to. If the product gate ever rises
// again while SELECTABLE tracks it, the real pixel cases above compile out and
// the suite gets quieter -- which is what the *_NAMEABLE bound is for. If it
// rises and SELECTABLE is NOT updated, they compile in and fail loudly on the
// black frame. Either way the divergence is visible without a pin.

TEST_CASE(
    "The MSB-aligned semi-planar family unpacks to the right colour",
    "[gfx][video][decoder][pixels]")
{
  // P010 / P210 / P410 (10-bit, 4:2:0 / 4:2:2 / 4:4:4) and P016 (16-bit) all
  // carry the 8-bit code left-aligned in a 16-bit word, so a UNORM16 sampler
  // divides by 65535 where the format's full scale is 65280 -- every level
  // 0.39% low, white 254 instead of 255. HWVAAPI and HWVulkan build their
  // 10-bit shader from P010Decoder::frag, so they inherit whatever this says.
  const auto api = GENERATE(from_range(platform_backends()));
  struct Case
  {
    const char* name;
    AVPixelFormat fmt;
    int bits, sx, sy;
  };
  const Case cases[]{
      {"p010le", AV_PIX_FMT_P010LE, 10, 2, 2},
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 17, 100)
      {"p210le", AV_PIX_FMT_P210LE, 10, 2, 1},
      {"p410le", AV_PIX_FMT_P410LE, 10, 1, 1},
#endif
      {"p016le", AV_PIX_FMT_P016LE, 16, 2, 2},
  };

  for(const auto& k : cases)
    for(const auto& c : kColors)
    {
      const auto out = render_camera(
          api, k.fmt,
          packSemiPlanarMsb([&](int) { return c.y; }, c.cb, c.cr, k.bits, k.sx, k.sy));
      INFO("backend " << out.backend);
      if(out.skipped)
        SKIP(out.skip_reason);
      REQUIRE(out.error.empty());
      check_color(out.img, c, k.name);
    }
}

TEST_CASE(
    "16-bit RGB and grey obey the same framing rule",
    "[gfx][video][decoder][pixels]")
{
  // No colour matrix here, so this isolates the framing rule from the matrix:
  // rgba64le / gray16le hold the 8-bit code in the top 8 bits, and ffmpeg reads
  // them back with `>> 8` -- `ffmpeg -pix_fmt rgb48le` turns word 51400 into 201,
  // not 200, which is 51400/65280 and not 51400/65535.
  const auto api = GENERATE(from_range(platform_backends()));
  {
    const auto out = render_camera(
        api, AV_PIX_FMT_RGBA64LE, packRgba64(64, 160, 200), AVCOL_SPC_RGB,
        AVCOL_RANGE_JPEG);
    INFO("backend " << out.backend);
    if(out.skipped)
      SKIP(out.skip_reason);
    REQUIRE(out.error.empty());
    REQUIRE(out.img.valid());
    const auto px = out.img.at(W / 2, H / 2);
    INFO("rgba64le: got (" << int(px[0]) << "," << int(px[1]) << "," << int(px[2]) << ")");
    CHECK(std::abs(int(px[0]) - 64) <= kTol);
    CHECK(std::abs(int(px[1]) - 160) <= kTol);
    CHECK(std::abs(int(px[2]) - 200) <= kTol);
  }
  {
    const auto out = render_camera(
        api, AV_PIX_FMT_GRAY16LE, packGray16([](int) { return 200; }), AVCOL_SPC_RGB,
        AVCOL_RANGE_JPEG);
    INFO("backend " << out.backend);
    if(out.skipped)
      SKIP(out.skip_reason);
    REQUIRE(out.error.empty());
    REQUIRE(out.img.valid());
    const auto px = out.img.at(W / 2, H / 2);
    INFO("gray16le: got (" << int(px[0]) << "," << int(px[1]) << "," << int(px[2]) << ")");
    CHECK(std::abs(int(px[0]) - 200) <= kTol);
    CHECK(std::abs(int(px[1]) - 200) <= kTol);
    CHECK(std::abs(int(px[2]) - 200) <= kTol);
  }
}

TEST_CASE("YUVA444P12Decoder unpacks four 12-bit planes", "[gfx][video][decoder][pixels]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  for(const auto& c : kColors)
  {
    const auto out = render_camera(
        api, AV_PIX_FMT_YUVA444P12LE,
        packYuva444p12([&](int) { return c.y; }, c.cb, c.cr));
    INFO("backend " << out.backend);
    if(out.skipped)
      SKIP(out.skip_reason);
    REQUIRE(out.error.empty());
    check_color(out.img, c, "yuva444p12le");
  }
}

// A horizontal luma staircase at constant grey chroma: four bands of 16
// columns, so R = G = B = Y in each. Every colour case above stays correct
// under a positional bug -- reading the wrong 10-bit field, the wrong half of a
// Y210 macropixel or the wrong row stride all reproduce a UNIFORM frame
// perfectly. This is what separates them.
//
// Y210 is 4:2:2, so the band width is even and both halves of every macropixel
// carry the same luma except at the band edges, which are not sampled.
namespace
{
constexpr int kBandLuma[4]{32, 96, 160, 224};

int rampLumaAt(int x)
{
  if(x < 0)
    x = 0;
  if(x >= W)
    x = W - 1;
  return kBandLuma[x / (W / 4)];
}

void check_ramp(
    score::gfx::GraphicsApi api, const char* name, AVPixelFormat fmt, Planes planes)
{
  const auto out = render_camera(api, fmt, std::move(planes));
  INFO("backend " << out.backend << " format " << name);
  if(out.skipped)
    SKIP(out.skip_reason);
  REQUIRE(out.error.empty());
  REQUIRE(out.img.valid());
  for(int b = 0; b < 4; b++)
  {
    const int x = b * (W / 4) + (W / 8); // band centre
    const auto px = out.img.at(x, H / 2);
    INFO(
        name << " band " << b << " at x=" << x << ": got " << int(px[0]) << ","
             << int(px[1]) << "," << int(px[2]) << " want " << kBandLuma[b]);
    CHECK(std::abs(int(px[0]) - kBandLuma[b]) <= kTol);
    CHECK(std::abs(int(px[1]) - kBandLuma[b]) <= kTol);
    CHECK(std::abs(int(px[2]) - kBandLuma[b]) <= kTol);
  }
}
}

TEST_CASE(
    "Packed decoders address the right sample for the right pixel",
    "[gfx][video][decoder][pixels]")
{
  const auto api = GENERATE(from_range(platform_backends()));

#if SCORE_TEST_XV30_SELECTABLE
  check_ramp(api, "xv30le", AV_PIX_FMT_XV30LE, packXv30(rampLumaAt, 128, 128));
#endif
#if SCORE_TEST_VUYA_SELECTABLE
  check_ramp(api, "vuya", AV_PIX_FMT_VUYA, packVuya(rampLumaAt, 128, 128));
#endif
  check_ramp(
      api, "yuva444p12le", AV_PIX_FMT_YUVA444P12LE,
      packYuva444p12(rampLumaAt, 128, 128));
  check_ramp(
      api, "p010le", AV_PIX_FMT_P010LE,
      packSemiPlanarMsb(rampLumaAt, 128, 128, 10, 2, 2));
  check_ramp(
      api, "p016le", AV_PIX_FMT_P016LE,
      packSemiPlanarMsb(rampLumaAt, 128, 128, 16, 2, 2));
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 17, 100)
  check_ramp(
      api, "p410le", AV_PIX_FMT_P410LE,
      packSemiPlanarMsb(rampLumaAt, 128, 128, 10, 1, 1));
#endif
}

#if SCORE_TEST_HAS_Y210
TEST_CASE(
    "Y210Decoder addresses the right sample for the right pixel",
    "[gfx][video][decoder][pixels]")
{
  // WAS [!shouldfail]. Kept isolated from the case above because under the
  // RGBA16F reinterpretation the staircase collapsed to two constants (bands
  // 0/2/3 near (0,84,0), band 1 saturated to white) and said nothing about
  // ADDRESSING. With the RG16 upload it does: the luma of pixel x is the .r of
  // texel x and the two chroma samples are the .g of the macropixel's pair, so
  // a half-macropixel or stride error moves a band.
  const auto api = GENERATE(from_range(platform_backends()));
  check_ramp(api, "y210le", AV_PIX_FMT_Y210LE, packY210(rampLumaAt, 128, 128));
}
#endif

TEST_CASE(
    "An unusable pixel format degrades instead of crashing",
    "[gfx][video][decoder][pixels]")
{
  // The reachable half of the hardware-decoder story. A HW AVPixelFormat needs a
  // real hw_frames_ctx and a live device, neither of which a synthesised frame
  // has, so those decoders cannot be exercised here at all (see the file
  // header). What CAN be exercised is the fallback createGpuDecoder() takes when
  // it has no decoder for a format: EmptyDecoder, whose fragment shader writes
  // nothing. The renderer must survive that and hand back a valid, black frame
  // rather than an assert or an unwritten target.
  //
  // AV_PIX_FMT_UYYVYY411 is used because no branch of the factory claims it and
  // it needs no device, so the path taken is exactly the unhandled-format one.
  const auto api = GENERATE(from_range(platform_backends()));

  Planes p;
  p.count = 1;
  p.linesize[0] = W * 2;
  p.data[0].assign(std::size_t(W) * H * 2, 0x80);

  const auto out = render_camera(api, AV_PIX_FMT_UYYVYY411, std::move(p));
  INFO("backend " << out.backend);
  if(out.skipped)
    SKIP(out.skip_reason);
  REQUIRE(out.error.empty());
  REQUIRE(out.img.valid());
  const auto px = out.img.at(W / 2, H / 2);
  CHECK(int(px[0]) == 0);
  CHECK(int(px[1]) == 0);
  CHECK(int(px[2]) == 0);
}
