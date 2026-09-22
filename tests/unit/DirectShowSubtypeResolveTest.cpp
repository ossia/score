// The DirectShow enumeration decision (9b688179e5, consumer half).
//
// tests/unit/VideoPixelFormatTest.cpp pins the shared fourcc TABLE with 30
// literal fourccs. What it cannot see is what CameraDevice.win32.cpp does with
// it: reduce a MEDIASUBTYPE GUID to a fourcc, resolve the layout with a
// chroma-swap fallback, and pick a codec. Those three steps are arithmetic over
// the GUID's bytes and now live in a portable header, so they are asserted here
// rather than only on the one platform where the enumeration compiles.

#include <Gfx/CameraFormatName.hpp>
#include <Gfx/Graph/interop/DirectShowSubtype.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>

extern "C" {
#include <libavutil/pixdesc.h>
}

using namespace score::gfx::interop;
using V = Video::VideoPixelFormat;

namespace
{
constexpr auto fcc = directShowFourcc;

//! The bit pattern of a YUV MEDIASUBTYPE, written out rather than derived, so
//! the expectation does not come from the code under test.
constexpr DirectShowGuid yuvSubtype(uint32_t data1)
{
  return DirectShowGuid{data1, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
}

std::string_view name(AVPixelFormat f)
{
  const char* n = av_get_pix_fmt_name(f);
  return n ? std::string_view{n} : std::string_view{"<none>"};
}

// The real SDK GUIDs, copied from the DirectShow headers.
constexpr DirectShowGuid MEDIASUBTYPE_RGB24{
    0xe436eb7d, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
constexpr DirectShowGuid MEDIASUBTYPE_ARGB1555{
    0x297c55af, 0xe209, 0x4cb3, {0xb7, 0x57, 0xc7, 0x6d, 0x6b, 0x9c, 0x88, 0xa8}};
}

TEST_CASE("a YUV subtype reduces to the fourcc in Data1", "[unit][dshow][video]")
{
  CHECK(directShowSubtypeFourcc(yuvSubtype(0x32315659)) == fcc('Y', 'V', '1', '2'));
  CHECK(directShowSubtypeFourcc(yuvSubtype(0x36313256)) == fcc('V', '2', '1', '6'));
  CHECK(directShowSubtypeFourcc(yuvSubtype(fcc('M', 'J', 'P', 'G'))) == fcc('M', 'J', 'P', 'G'));
  CHECK(directShowSubtypeFourcc(yuvSubtype(fcc('P', '4', '0', '8'))) == fcc('P', '4', '0', '8'));
}

TEST_CASE("an RGB SDK GUID is never mistaken for a fourcc", "[unit][dshow][video]")
{
  CHECK(directShowSubtypeFourcc(MEDIASUBTYPE_RGB24) == 0u);
  CHECK(directShowSubtypeFourcc(MEDIASUBTYPE_ARGB1555) == 0u);

  // A GUID whose Data1 looks like a fourcc but whose suffix does not match is
  // still not one: only the exact YUV shape qualifies.
  auto almost = yuvSubtype(fcc('Y', 'V', '1', '2'));
  almost.Data4[7] = 0x72;
  CHECK(directShowSubtypeFourcc(almost) == 0u);

  auto wrongData3 = yuvSubtype(fcc('Y', 'V', '1', '2'));
  wrongData3.Data3 = 0x0011;
  CHECK(directShowSubtypeFourcc(wrongData3) == 0u);
}

TEST_CASE("a subtype with no fourcc has no layout", "[unit][dshow][video]")
{
  CHECK(directShowSubtypePixelFormat(MEDIASUBTYPE_RGB24) == AV_PIX_FMT_NONE);
  CHECK(directShowSubtypeFourcc(MEDIASUBTYPE_RGB24) == 0u);
  // An unhandled fourcc likewise: the RGB chain in the enumeration is reached
  // by falling through, so this must not answer something plausible.
  CHECK(
      directShowSubtypePixelFormat(yuvSubtype(fcc('Z', 'Z', 'Z', 'Z')))
      == AV_PIX_FMT_NONE);
}

TEST_CASE("the V-before-U layouts resolve through their twin", "[unit][dshow][video]")
{
  // The layout is recorded honestly...
  CHECK(fromDirectShowFourcc(fcc('Y', 'V', '1', '2')) == V::YVU420P);
  CHECK(chromaSwappedTwin(V::YVU420P) == V::YUV420P);
  CHECK(toAVPixelFormat(V::YVU420P) == AV_PIX_FMT_NONE);
  // ...and the enumeration still offers the camera, under the twin's name.
  CHECK(name(directShowSubtypePixelFormat(yuvSubtype(0x32315659))) == name(av_get_pix_fmt("yuv420p")));

  CHECK(fromDirectShowFourcc(fcc('Y', 'V', 'U', '9')) == V::YVU410P);
  CHECK(chromaSwappedTwin(V::YVU410P) == V::YUV410P);
  CHECK(name(directShowSubtypePixelFormat(yuvSubtype(fcc('Y', 'V', 'U', '9'))))
        == std::string_view{"yuv410p"});

  CHECK(fromDirectShowFourcc(fcc('Y', 'V', '1', '6')) == V::YVU422P);
  CHECK(name(directShowSubtypePixelFormat(yuvSubtype(fcc('Y', 'V', '1', '6'))))
        == std::string_view{"yuv422p"});
}

TEST_CASE("YV12 and I420 do not collapse onto one another", "[unit][dshow][video]")
{
  // Both end up offered as yuv420p, but through different layouts: naming YV12
  // YUV420P directly is what exchanged red and blue.
  CHECK(fromDirectShowFourcc(fcc('I', '4', '2', '0')) == V::YUV420P);
  CHECK(fromDirectShowFourcc(fcc('Y', 'V', '1', '2')) == V::YVU420P);
  CHECK(fromDirectShowFourcc(fcc('I', '4', '2', '0'))
        != fromDirectShowFourcc(fcc('Y', 'V', '1', '2')));
}

TEST_CASE("V216 keeps its own component order", "[unit][dshow][video]")
{
  CHECK(fromDirectShowFourcc(fcc('V', '2', '1', '6')) == V::V216);
  CHECK(fromDirectShowFourcc(fcc('Y', '2', '1', '6')) == V::Y216);
  CHECK(fromDirectShowFourcc(fcc('V', '2', '1', '6'))
        != fromDirectShowFourcc(fcc('Y', '2', '1', '6')));
}

TEST_CASE("the compressed subtypes are recognised as such", "[unit][dshow][video]")
{
  for(auto f : {fcc('M', 'J', 'P', 'G'), fcc('T', 'V', 'M', 'J'),
                fcc('W', 'A', 'K', 'E'), fcc('P', 'l', 'u', 'm'),
                fcc('C', 'F', 'C', 'C'), fcc('I', 'J', 'P', 'G'),
                fcc('H', '2', '6', '4'), fcc('H', '2', '6', '5'),
                fcc('H', 'E', 'V', 'C'), fcc('d', 'v', 's', 'd'),
                fcc('M', 'D', 'V', 'F'), fcc('V', 'P', '9', '0'),
                fcc('A', 'V', '0', '1'), fcc('M', 'P', '4', 'V')})
  {
    INFO("fourcc " << f);
    CHECK(directShowSubtypeIsCompressed(yuvSubtype(f)));
    // A compressed subtype never yields a raw layout.
    CHECK(directShowSubtypePixelFormat(yuvSubtype(f)) == AV_PIX_FMT_NONE);
  }

  CHECK_FALSE(directShowSubtypeIsCompressed(yuvSubtype(fcc('N', 'V', '1', '2'))));
  CHECK_FALSE(directShowSubtypeIsCompressed(MEDIASUBTYPE_RGB24));
}

TEST_CASE("MJPEG-family subtypes resolve to the MJPEG decoder", "[unit][dshow][video]")
{
  // The six Motion-JPEG spellings dshow.h documents.
  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('M', 'J', 'P', 'G'))) == AV_CODEC_ID_MJPEG);
  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('T', 'V', 'M', 'J'))) == AV_CODEC_ID_MJPEG);
  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('W', 'A', 'K', 'E'))) == AV_CODEC_ID_MJPEG);
  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('P', 'l', 'u', 'm'))) == AV_CODEC_ID_MJPEG);
  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('C', 'F', 'C', 'C'))) == AV_CODEC_ID_MJPEG);
  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('I', 'J', 'P', 'G'))) == AV_CODEC_ID_MJPEG);

  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('N', 'V', '1', '2'))) == AV_CODEC_ID_RAWVIDEO);
  CHECK(directShowSubtypeCodec(MEDIASUBTYPE_RGB24) == AV_CODEC_ID_RAWVIDEO);
}

// isDirectShowCompressedFourcc knows about H264, and directShowSubtypeCodec
// dispatches on the fourcc rather than collapsing every compressed subtype to
// MJPEG, so an H.264 camera is offered with the H.264 decoder.
TEST_CASE(
    "an H264 subtype resolves to the H264 decoder",
    "[unit][dshow][video]")
{
  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('H', '2', '6', '4'))) == AV_CODEC_ID_H264);
}

TEST_CASE("the raw subtypes resolve to the format FFmpeg names", "[unit][dshow][video]")
{
  struct Pin
  {
    uint32_t fourcc;
    const char* ffmpeg;
  };
  // Expectations resolved through av_get_pix_fmt() rather than retyped
  // enumerators, so they come from FFmpeg and not from the table under test.
  static constexpr Pin pins[]{
      {fcc('Y', 'U', 'Y', '2'), "yuyv422"}, {fcc('U', 'Y', 'V', 'Y'), "uyvy422"},
      {fcc('Y', 'V', 'Y', 'U'), "yvyu422"}, {fcc('I', '4', '2', '0'), "yuv420p"},
      {fcc('N', 'V', '1', '2'), "nv12"},    {fcc('N', 'V', '2', '1'), "nv21"},
      {fcc('P', '0', '1', '0'), "p010le"},  {fcc('Y', '2', '1', '0'), "y210le"},
      {fcc('P', '4', '0', '8'), "nv24"},
  };
  for(auto [f, n] : pins)
  {
    INFO("fourcc " << f << " expected " << n);
    const auto expected = av_get_pix_fmt(n);
    REQUIRE(expected != AV_PIX_FMT_NONE);
    CHECK(directShowSubtypePixelFormat(yuvSubtype(f)) == expected);
  }
}

// A compressed pin reaches the description builder as {codec, AV_PIX_FMT_NONE},
// and av_get_pix_fmt_name(AV_PIX_FMT_NONE) is nullptr, so the name has to come
// from the codec for every codec directShowSubtypeCodec can answer.
TEST_CASE("every compressed subtype has a name to be offered under", "[unit][dshow][video]")
{
  for(auto f : {fcc('H', '2', '6', '4'), fcc('M', 'J', 'P', 'G'),
                fcc('T', 'V', 'M', 'J'), fcc('W', 'A', 'K', 'E'),
                fcc('P', 'l', 'u', 'm'), fcc('d', 'v', 's', 'd')})
  {
    INFO("fourcc " << f);
    const auto subtype = yuvSubtype(f);
    REQUIRE(directShowSubtypeIsCompressed(subtype));

    // What enumerateCameraFormat() stores for a compressed subtype: the codec
    // it resolves to, and no pixel format at all.
    const auto codec = directShowSubtypeCodec(subtype);
    REQUIRE(directShowSubtypePixelFormat(subtype) == AV_PIX_FMT_NONE);
    REQUIRE(av_get_pix_fmt_name(AV_PIX_FMT_NONE) == nullptr);

    const auto named = Gfx::cameraFormatName(codec, AV_PIX_FMT_NONE);
    CHECK_FALSE(named.empty());
    CHECK(named != "unknown");
    // Named after the stream, not after a layout it does not have.
    CHECK(named == std::string_view{avcodec_descriptor_get(codec)->name});
  }

  CHECK(Gfx::cameraFormatName(AV_CODEC_ID_H264, AV_PIX_FMT_NONE) == "h264");
  CHECK(Gfx::cameraFormatName(AV_CODEC_ID_MJPEG, AV_PIX_FMT_NONE) == "mjpeg");
}

TEST_CASE("a raw subtype is named by its pixel format", "[unit][dshow][video]")
{
  // enumerateCameraFormat() leaves the codec at RAWVIDEO and fills the layout.
  const auto nv12 = yuvSubtype(fcc('N', 'V', '1', '2'));
  REQUIRE(directShowSubtypeCodec(nv12) == AV_CODEC_ID_RAWVIDEO);
  CHECK(
      Gfx::cameraFormatName(AV_CODEC_ID_RAWVIDEO, directShowSubtypePixelFormat(nv12))
      == "nv12");

  // Neither codec nor layout still yields a usable string.
  CHECK(Gfx::cameraFormatName(AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_NONE) == "unknown");
  CHECK(Gfx::cameraFormatName(AV_CODEC_ID_NONE, AV_PIX_FMT_NONE) == "unknown");
}

// The compressed half of the decision is FFmpeg's RIFF video-tag table, which
// covers the spellings a DirectShow filter can present. Pinned here because a
// short hand-written list would pass the H.264 case above and fail these.
TEST_CASE(
    "the compressed spellings FFmpeg knows are all resolved",
    "[unit][dshow][video]")
{
  struct Pin
  {
    uint32_t fourcc;
    AVCodecID codec;
  };
  static const Pin pins[]{
      // H.264, as capture cards and IP-camera filters spell it.
      {fcc('H', '2', '6', '4'), AV_CODEC_ID_H264},
      {fcc('h', '2', '6', '4'), AV_CODEC_ID_H264},
      {fcc('a', 'v', 'c', '1'), AV_CODEC_ID_H264},
      {fcc('X', '2', '6', '4'), AV_CODEC_ID_H264},
      // DV, in its five spellings.
      {fcc('d', 'v', 's', 'd'), AV_CODEC_ID_DVVIDEO},
      {fcc('D', 'V', 'S', 'D'), AV_CODEC_ID_DVVIDEO},
      {fcc('d', 'v', 'h', 'd'), AV_CODEC_ID_DVVIDEO},
      {fcc('D', 'V', 'C', 'S'), AV_CODEC_ID_DVVIDEO},
      // Encoders that expose a DirectShow source.
      {fcc('V', 'P', '8', '0'), AV_CODEC_ID_VP8},
      {fcc('V', 'P', '9', '0'), AV_CODEC_ID_VP9},
      {fcc('A', 'V', '0', '1'), AV_CODEC_ID_AV1},
      {fcc('M', 'P', '4', 'V'), AV_CODEC_ID_MPEG4},
      {fcc('D', 'I', 'V', 'X'), AV_CODEC_ID_MPEG4},
      {fcc('X', 'V', 'I', 'D'), AV_CODEC_ID_MPEG4},
      {fcc('H', '2', '6', '3'), AV_CODEC_ID_H263},
      {fcc('M', 'J', '2', 'C'), AV_CODEC_ID_JPEG2000},
      // 10-bit and lossless card formats FFmpeg treats as codecs, not layouts.
      {fcc('v', '2', '1', '0'), AV_CODEC_ID_V210},
      {fcc('F', 'F', 'V', '1'), AV_CODEC_ID_FFV1},
      {fcc('H', 'F', 'Y', 'U'), AV_CODEC_ID_HUFFYUV},
  };
  for(auto [f, c] : pins)
  {
    INFO("fourcc " << f);
    CHECK(directShowSubtypeIsCompressed(yuvSubtype(f)));
    CHECK(directShowSubtypeCodec(yuvSubtype(f)) == c);
    CHECK_FALSE(Gfx::cameraFormatName(c, AV_PIX_FMT_NONE).empty());
  }
}

// H.265 is absent from FFmpeg's RIFF table, and the two spellings in the wild
// disagree: UVC cameras label the pin {35363248-...} = 'H265', OBS and the LAV
// filters {43564548-...} = 'HEVC' (LAVFilters issue #436). A camera whose only
// pin is H.265 has no format to offer unless both resolve.
TEST_CASE("both H.265 spellings resolve to the HEVC decoder", "[unit][dshow][video]")
{
  // The GUIDs as the devices present them, not built from a fourcc.
  constexpr DirectShowGuid uvcH265{
      0x35363248, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
  constexpr DirectShowGuid lavHevc{
      0x43564548, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

  CHECK(directShowSubtypeIsCompressed(uvcH265));
  CHECK(directShowSubtypeIsCompressed(lavHevc));
  CHECK(directShowSubtypeCodec(uvcH265) == AV_CODEC_ID_HEVC);
  CHECK(directShowSubtypeCodec(lavHevc) == AV_CODEC_ID_HEVC);
  CHECK(Gfx::cameraFormatName(AV_CODEC_ID_HEVC, AV_PIX_FMT_NONE) == "hevc");
}

// The packed-4:2:2 aliases: one layout, many fourccs. HDYC is the one capture
// cards present, and a subtype missing from the table is offered by nothing.
TEST_CASE("the packed 4:2:2 aliases all name one layout", "[unit][dshow][video]")
{
  for(auto f : {fcc('Y', 'U', 'Y', '2'), fcc('Y', 'U', 'Y', 'V'),
                fcc('Y', 'U', 'N', 'V'), fcc('V', '4', '2', '2'),
                fcc('y', 'u', 'v', 's')})
  {
    INFO("fourcc " << f);
    CHECK(name(directShowSubtypePixelFormat(yuvSubtype(f))) == std::string_view{"yuyv422"});
    CHECK_FALSE(directShowSubtypeIsCompressed(yuvSubtype(f)));
  }

  for(auto f : {fcc('U', 'Y', 'V', 'Y'), fcc('Y', '4', '2', '2'),
                fcc('H', 'D', 'Y', 'C'), fcc('U', 'Y', 'N', 'V'),
                fcc('u', 'y', 'v', '1'), fcc('2', 'v', 'u', 'y')})
  {
    INFO("fourcc " << f);
    CHECK(name(directShowSubtypePixelFormat(yuvSubtype(f))) == std::string_view{"uyvy422"});
    CHECK_FALSE(directShowSubtypeIsCompressed(yuvSubtype(f)));
  }
}

// A raw layout stays raw even when FFmpeg's RIFF table calls the same fourcc a
// codec: Y41P is the y41p decoder there, a pixel format here. The order the
// two tables are consulted in is what decides it.
TEST_CASE("a named layout wins over the RIFF table", "[unit][dshow][video]")
{
  for(auto f : {fcc('Y', '4', '1', 'P'), fcc('Y', '4', '1', '1')})
  {
    INFO("fourcc " << f);
    CHECK_FALSE(directShowSubtypeIsCompressed(yuvSubtype(f)));
    CHECK(directShowSubtypeCodec(yuvSubtype(f)) == AV_CODEC_ID_RAWVIDEO);
    CHECK(name(directShowSubtypePixelFormat(yuvSubtype(f)))
          == std::string_view{"uyyvyy411"});
  }

  // The planar AVI spellings, and the U-before-V twin of YVU9.
  CHECK(name(directShowSubtypePixelFormat(yuvSubtype(fcc('Y', '4', '2', 'B'))))
        == std::string_view{"yuv422p"});
  CHECK(name(directShowSubtypePixelFormat(yuvSubtype(fcc('Y', '4', '1', 'B'))))
        == std::string_view{"yuv411p"});
  CHECK(fromDirectShowFourcc(fcc('Y', 'U', 'V', '9')) == V::YUV410P);
  CHECK(fromDirectShowFourcc(fcc('Y', 'V', 'U', '9')) == V::YVU410P);
}

// An unknown fourcc stays unknown: that is what tells enumerateCameraFormat to
// skip the format rather than offer one nothing can open.
TEST_CASE("an unknown fourcc resolves to nothing", "[unit][dshow][video]")
{
  for(auto f : {fcc('Z', 'Z', 'Z', 'Z'), fcc('I', 'N', 'V', 'Z'), 0u})
  {
    INFO("fourcc " << f);
    CHECK(directShowFourccCodec(f) == AV_CODEC_ID_NONE);
    CHECK_FALSE(directShowSubtypeIsCompressed(yuvSubtype(f)));
    CHECK(directShowSubtypePixelFormat(yuvSubtype(f)) == AV_PIX_FMT_NONE);
  }
}
