// Unit tests for Gfx/Graph/interop/DrmFourcc.hpp: the DRM fourcc <-> AVPixel-
// Format bridge the dma-buf and PipeWire paths go through.
//
// This is NOT the header tests/unit/VideoPixelFormatTest.cpp covers. That one
// tests DrmPixelFormat.hpp (fromDrmFourcc / toDrmFourcc, fourcc <-> layout);
// this one tests the three functions layered on top of it -- drmFourccToAv,
// avToDrmFourcc, drmFourccToVideoPixelFormat -- which were rewritten from a
// hand-maintained table into delegations, with nothing but a commit message
// asserting the answers had not moved.
//
// So the expectations here are literal AVPixelFormat constants rather than
// toAVPixelFormat(...) of the pinned layout: routed through the vocabulary the
// assertion would agree with itself no matter what the vocabulary said.

#include <Gfx/Graph/interop/DrmFourcc.hpp>

#include <catch2/catch_test_macros.hpp>

namespace vpf = score::gfx::interop;
using V = vpf::VideoPixelFormat;

namespace
{
constexpr std::uint32_t f(const char* s) noexcept
{
  return vpf::drmFourcc(s[0], s[1], s[2], s[3]);
}
} // namespace

// DRM_FORMAT_GR1616 is fourcc_code('G','R','3','2'). 'GR16' -- the literal the
// two duplicated copies of this table used to carry -- is not a fourcc any
// kernel defines, so a plane imported with it was refused rather than sampled.
TEST_CASE("the single- and dual-channel plane fourccs", "[gfx][drm]")
{
  CHECK(vpf::DRM_R8 == 0x20203852u);
  CHECK(vpf::DRM_GR88 == 0x38385247u);
  CHECK(vpf::DRM_R16 == 0x20363152u);
  CHECK(vpf::DRM_GR1616 == 0x32335247u);
  CHECK(vpf::DRM_GR1616 == f("GR32"));
  CHECK(vpf::DRM_GR1616 != f("GR16"));
}

TEST_CASE("DRM fourcc to AVPixelFormat", "[gfx][drm]")
{
  struct Pin
  {
    const char* fourcc;
    AVPixelFormat av;
  };
  // A DRM name reads from the most significant byte of the machine word down,
  // so the memory order is its reverse: 'AR24' is B,G,R,A in memory and is
  // therefore FFmpeg's BGRA.
  static const Pin kPins[] = {
      {"AR24", AV_PIX_FMT_BGRA},        {"AB24", AV_PIX_FMT_RGBA},
      {"XR24", AV_PIX_FMT_BGR0},        {"XB24", AV_PIX_FMT_RGB0},
      {"RA24", AV_PIX_FMT_ABGR},        {"BA24", AV_PIX_FMT_ARGB},
      {"RG24", AV_PIX_FMT_BGR24},       {"BG24", AV_PIX_FMT_RGB24},
      {"RG16", AV_PIX_FMT_RGB565LE},    {"AR30", AV_PIX_FMT_X2RGB10LE},
      {"AB30", AV_PIX_FMT_X2BGR10LE},   {"NV12", AV_PIX_FMT_NV12},
      {"NV21", AV_PIX_FMT_NV21},        {"NV16", AV_PIX_FMT_NV16},
      {"NV24", AV_PIX_FMT_NV24},        {"NV42", AV_PIX_FMT_NV42},
      {"P010", AV_PIX_FMT_P010LE},      {"P210", AV_PIX_FMT_P210LE},
      {"P410", AV_PIX_FMT_P416LE},      {"YU12", AV_PIX_FMT_YUV420P},
      {"YU16", AV_PIX_FMT_YUV422P},     {"YU24", AV_PIX_FMT_YUV444P},
      {"YUYV", AV_PIX_FMT_YUYV422},     {"YVYU", AV_PIX_FMT_YVYU422},
      {"UYVY", AV_PIX_FMT_UYVY422},     {"R8  ", AV_PIX_FMT_GRAY8},
      {"R16 ", AV_PIX_FMT_GRAY16LE},
  };
  for(const auto& p : kPins)
  {
    INFO("DRM fourcc " << p.fourcc);
    CHECK(vpf::drmFourccToAv(f(p.fourcc)) == p.av);
    CHECK(vpf::avToDrmFourcc(p.av) == f(p.fourcc));
  }
}

// FFmpeg expresses a V-before-U plane order by exchanging the U and V pointers
// rather than by having a pixel format for it, so there is nothing here to
// answer with. Naming yuv420p would silently swap chroma; the previous table
// returned AV_PIX_FMT_NONE plus a comment telling callers to swap, and the
// delegation deliberately keeps the same answer.
TEST_CASE("layouts FFmpeg cannot name answer NONE", "[gfx][drm]")
{
  const char* unnameable[] = {"YV12", "YV16", "VYUY", "NV61", "AB4H"};
  for(const char* s : unnameable)
  {
    INFO("DRM fourcc " << s);
    CHECK(vpf::drmFourccToAv(f(s)) == AV_PIX_FMT_NONE);
  }
}

// What the callers that import a dma-buf directly ask for: the layout, not a
// decode target. This is where the plane-swapped orders stop being a dead end.
TEST_CASE("DRM fourcc to the buffer layout", "[gfx][drm]")
{
  CHECK(vpf::drmFourccToVideoPixelFormat(vpf::DRM_YVU420) == V::YVU420P);
  CHECK(vpf::drmFourccToVideoPixelFormat(vpf::DRM_YUV420) == V::YUV420P);
  CHECK(vpf::drmFourccToVideoPixelFormat(vpf::DRM_NV12) == V::NV12);
  CHECK(vpf::drmFourccToVideoPixelFormat(vpf::DRM_ARGB8888) == V::BGRA8);
  CHECK(vpf::drmFourccToVideoPixelFormat(vpf::DRM_ABGR8888) == V::RGBA8);
  CHECK(vpf::drmFourccToVideoPixelFormat(vpf::DRM_ARGB2101010) == V::X2RGB10);
  CHECK(vpf::drmFourccToVideoPixelFormat(vpf::DRM_ABGR2101010) == V::X2BGR10);
  CHECK(vpf::drmFourccToVideoPixelFormat(vpf::DRM_BGR888) == V::RGB24);
}

TEST_CASE("unmapped fourccs and formats", "[gfx][drm]")
{
  CHECK(vpf::drmFourccToAv(0) == AV_PIX_FMT_NONE);
  CHECK(vpf::drmFourccToAv(f("ZZZZ")) == AV_PIX_FMT_NONE);
  CHECK(vpf::drmFourccToVideoPixelFormat(0) == V::Unknown);
  CHECK(vpf::drmFourccToVideoPixelFormat(f("ZZZZ")) == V::Unknown);
  CHECK(vpf::avToDrmFourcc(AV_PIX_FMT_NONE) == 0u);
  CHECK(vpf::avToDrmFourcc(AV_PIX_FMT_DRM_PRIME) == 0u);
}
