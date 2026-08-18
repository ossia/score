// Unit tests for Gfx/Pipewire/PipewireFormats.hpp -- the ten inline mapping
// functions the PipeWire video input and output devices negotiate through --
// and for the SPA half of Gfx/Graph/interop/DrmFourcc.hpp, which only compiles
// where the SPA headers are present and is therefore tested here.
//
// The device pair itself needs a running daemon and stays with the
// hardware-gated PipewireRoundtrip harness. Everything below is a table.
//
// The assertion with teeth is the last one: PipeWire's own tag -> fourcc table
// and the interop vocabulary must not be able to disagree. They already did
// once -- RGB10A2 was exported as 'AB30', the mirrored layout of the 'AR30' it
// negotiated -- and nothing caught it because each table was self-consistent.

#include <Gfx/Pipewire/PipewireFormats.hpp>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

#include <catch2/catch_test_macros.hpp>

namespace pwf = Gfx::PipeWire::formats;
namespace vpf = score::gfx::interop;
using pwf::Tag;

namespace
{
constexpr Tag kTags[] = {Tag::RGBA8,   Tag::BGRA8,   Tag::RGB10A2,
                         Tag::BGR10A2, Tag::RGBA16F, Tag::RGBA32F,
                         Tag::P010,    Tag::P210,    Tag::YUV420P,
                         Tag::YV12,    Tag::NV12,    Tag::YUYV422,
                         Tag::UYVY422, Tag::RGB24};
} // namespace

// P210 has no SPA enumerator at all, so toSpa() aliases it onto P010 and the
// way back cannot tell them apart. Tag::Unknown falls back to RGBA rather than
// to a sentinel. Both are asserted rather than excluded, so that giving P210 a
// real SPA value some day is a test failure and not a silent behaviour change.
TEST_CASE("tag <-> SPA round-trips, and the two that cannot", "[gfx][pipewire]")
{
  for(Tag t : kTags)
  {
    if(t == Tag::P210)
      continue;
    INFO("tag " << int(t));
    CHECK(pwf::tagFromSpa(pwf::toSpa(t)) == t);
  }
  CHECK(pwf::toSpa(Tag::P210) == pwf::toSpa(Tag::P010));
  CHECK(pwf::tagFromSpa(pwf::toSpa(Tag::P210)) == Tag::P010);
  CHECK(pwf::toSpa(Tag::Unknown) == SPA_VIDEO_FORMAT_RGBA);
  CHECK(pwf::tagFromSpa(SPA_VIDEO_FORMAT_UNKNOWN) == Tag::Unknown);
  CHECK(pwf::tagFromSpa(0xFFFFu) == Tag::Unknown);
}

TEST_CASE("tag names parse, case-insensitively", "[gfx][pipewire]")
{
  struct Pin
  {
    const char* text;
    Tag tag;
  };
  static const Pin kPins[] = {
      {"rgba8", Tag::RGBA8},     {"rgba", Tag::RGBA8},
      {"RGBA8", Tag::RGBA8},     {"bgra8", Tag::BGRA8},
      {"bgra", Tag::BGRA8},      {"rgb10a2", Tag::RGB10A2},
      {"bgr10a2", Tag::BGR10A2}, {"rgba16f", Tag::RGBA16F},
      {"rgba_f16", Tag::RGBA16F},{"rgba32f", Tag::RGBA32F},
      {"rgba_f32", Tag::RGBA32F},{"p010", Tag::P010},
      {"p210", Tag::P210},       {"yuv420p", Tag::YUV420P},
      {"i420", Tag::YUV420P},    {"yv12", Tag::YV12},
      {"nv12", Tag::NV12},       {"yuyv422", Tag::YUYV422},
      {"yuy2", Tag::YUYV422},    {"uyvy422", Tag::UYVY422},
      {"uyvy", Tag::UYVY422},    {"rgb24", Tag::RGB24},
      {"rgb", Tag::RGB24},
  };
  for(const auto& p : kPins)
  {
    INFO("spelling " << p.text);
    CHECK(pwf::tagFromString(QString::fromUtf8(p.text)) == p.tag);
  }
  CHECK(pwf::tagFromString("") == Tag::Unknown);
  CHECK(pwf::tagFromString("rgba9") == Tag::Unknown);
}

// YV12 publishes as yuv420p because the copy path exchanges the plane pointers,
// so the AVPixelFormat cannot name it back; RGBA32F has no AVFrame form at all.
TEST_CASE("tag <-> AVPixelFormat", "[gfx][pipewire]")
{
  for(Tag t : kTags)
  {
    const auto av = pwf::toAvPixFmt(t);
    INFO("tag " << int(t));
    if(t == Tag::RGBA32F)
    {
      CHECK(av == AV_PIX_FMT_NONE);
      continue;
    }
    REQUIRE(av != AV_PIX_FMT_NONE);
    CHECK(pwf::tagFromAvPixFmt(av) == (t == Tag::YV12 ? Tag::YUV420P : t));
  }
  CHECK(pwf::toAvPixFmt(Tag::Unknown) == AV_PIX_FMT_NONE);
  CHECK(pwf::tagFromAvPixFmt(AV_PIX_FMT_NONE) == Tag::Unknown);
  CHECK(pwf::tagFromAvPixFmt(AV_PIX_FMT_GBRPF32LE) == Tag::Unknown);
}

// frameBytes() sizes SPA_PARAM_BUFFERS_size and validates the chunk sizes the
// other end sends, so it has to agree with what FFmpeg thinks a tight frame of
// the same format weighs -- not with bytesPerPixel(), which only knows the Y
// plane.
TEST_CASE("frameBytes matches FFmpeg's tight frame size", "[gfx][pipewire]")
{
  const int sizes[][2] = {{1920, 1080}, {640, 480}, {2, 2}};
  for(Tag t : kTags)
  {
    const auto av = pwf::toAvPixFmt(t);
    if(av == AV_PIX_FMT_NONE)
      continue;
    for(const auto& wh : sizes)
    {
      INFO("tag " << int(t) << " at " << wh[0] << "x" << wh[1]);
      CHECK(
          std::int64_t(pwf::frameBytes(t, wh[0], wh[1]))
          == av_image_get_buffer_size(av, wh[0], wh[1], 1));
    }
  }
  CHECK(pwf::frameBytes(Tag::RGBA32F, 16, 16) == 16u * 16u * 16u);
}

TEST_CASE("isPlanar agrees with FFmpeg's plane count", "[gfx][pipewire]")
{
  for(Tag t : kTags)
  {
    const auto av = pwf::toAvPixFmt(t);
    if(av == AV_PIX_FMT_NONE)
      continue;
    INFO("tag " << int(t));
    CHECK(pwf::isPlanar(t) == (av_pix_fmt_count_planes(av) > 1));
  }
  CHECK_FALSE(pwf::isPlanar(Tag::RGBA32F));
  CHECK_FALSE(pwf::isPlanar(Tag::Unknown));
}

TEST_CASE("QRhi targets exist only for the packed RGB tags", "[gfx][pipewire]")
{
  CHECK(pwf::toQRhi(Tag::RGBA8) == QRhiTexture::RGBA8);
  CHECK(pwf::toQRhi(Tag::BGRA8) == QRhiTexture::BGRA8);
  CHECK(pwf::toQRhi(Tag::RGB10A2) == QRhiTexture::RGB10A2);
  CHECK(pwf::toQRhi(Tag::BGR10A2) == QRhiTexture::RGB10A2);
  CHECK(pwf::toQRhi(Tag::RGBA16F) == QRhiTexture::RGBA16F);
  CHECK(pwf::toQRhi(Tag::RGBA32F) == QRhiTexture::RGBA32F);
  // Nothing in QRhi stores a planar layout, so the YUV tags fall back rather
  // than naming a texture format that would silently reinterpret the bytes.
  CHECK(pwf::toQRhi(Tag::NV12) == QRhiTexture::RGBA8);
  CHECK(pwf::toQRhi(Tag::P010) == QRhiTexture::RGBA8);
  CHECK(pwf::toQRhi(Tag::Unknown) == QRhiTexture::RGBA8);
}

TEST_CASE("the SPA route and the fourcc route answer the same", "[gfx][pipewire]")
{
  for(Tag t : kTags)
  {
    // P210 is the one tag with no SPA enumerator; toDrmFourcc names it
    // directly, which is exactly why it cannot be cross-checked this way.
    if(t == Tag::P210)
      continue;
    INFO("tag " << int(t));
    CHECK(
        pwf::toDrmFourcc(t)
        == vpf::toDrmFourcc(vpf::spaToVideoPixelFormat(pwf::toSpa(t))));
  }
  CHECK(pwf::toDrmFourcc(Tag::P210) == vpf::DRM_P210);
  CHECK(pwf::toDrmFourcc(Tag::RGB10A2) == vpf::DRM_ARGB2101010);
  CHECK(pwf::toDrmFourcc(Tag::BGR10A2) == vpf::DRM_ABGR2101010);
  CHECK(pwf::toDrmFourcc(Tag::RGBA32F) == 0u);
}

TEST_CASE("SPA formats resolve to the layout their fourcc names", "[gfx][pipewire]")
{
  using V = vpf::VideoPixelFormat;
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_RGBA) == V::RGBA8);
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_BGRA) == V::BGRA8);
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_RGBx) == V::RGBX8);
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_BGRx) == V::BGRX8);
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_xRGB_210LE) == V::X2RGB10);
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_xBGR_210LE) == V::X2BGR10);
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_RGB) == V::RGB24);
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_NV12) == V::NV12);
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_I420) == V::YUV420P);
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_YV12) == V::YVU420P);
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_YUY2) == V::YUYV422);
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_UYVY) == V::UYVY422);
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_UNKNOWN) == V::Unknown);
  CHECK(vpf::spaToVideoPixelFormat(SPA_VIDEO_FORMAT_GRAY8) == V::Unknown);
  CHECK(vpf::spaToDrmFourcc(SPA_VIDEO_FORMAT_GRAY8) == 0u);
}
