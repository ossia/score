// The DirectShow enumeration decision (9b688179e5, consumer half).
//
// tests/unit/VideoPixelFormatTest.cpp pins the shared fourcc TABLE with 30
// literal fourccs. What it cannot see is what CameraDevice.win32.cpp does with
// it: reduce a MEDIASUBTYPE GUID to a fourcc, resolve the layout with a
// chroma-swap fallback, and pick a codec. Those three steps are arithmetic over
// the GUID's bytes and now live in a portable header, so they are asserted here
// rather than only on the one platform where the enumeration compiles.

#include <Gfx/Graph/interop/DirectShowSubtype.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>

extern "C" {
#include <libavutil/pixdesc.h>
}

using namespace score::gfx::interop;
using V = VideoPixelFormat;

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
                fcc('H', '2', '6', '4')})
  {
    INFO("fourcc " << f);
    CHECK(directShowSubtypeIsCompressed(yuvSubtype(f)));
    // A compressed subtype never yields a raw layout.
    CHECK(directShowSubtypePixelFormat(yuvSubtype(f)) == AV_PIX_FMT_NONE);
  }

  CHECK_FALSE(directShowSubtypeIsCompressed(yuvSubtype(fcc('N', 'V', '1', '2'))));
  CHECK_FALSE(directShowSubtypeIsCompressed(MEDIASUBTYPE_RGB24));
}

TEST_CASE("every compressed subtype is offered as MJPEG, H264 included", "[unit][dshow][video]")
{
  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('M', 'J', 'P', 'G'))) == AV_CODEC_ID_MJPEG);
  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('T', 'V', 'M', 'J'))) == AV_CODEC_ID_MJPEG);
  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('W', 'A', 'K', 'E'))) == AV_CODEC_ID_MJPEG);
  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('P', 'l', 'u', 'm'))) == AV_CODEC_ID_MJPEG);

  // Recorded as a defect, not as a specification. 9b688179e5 taught
  // isDirectShowCompressedFourcc about H264, and enumerateCameraFormat hands
  // every compressed subtype to the MJPEG decoder, so an H.264 camera is
  // offered with the wrong decoder. Fixing it means dispatching on the fourcc
  // in directShowSubtypeCodec, at which point this line goes red and names the
  // change.
  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('H', '2', '6', '4'))) == AV_CODEC_ID_MJPEG);
  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('H', '2', '6', '4'))) != AV_CODEC_ID_H264);

  CHECK(directShowSubtypeCodec(yuvSubtype(fcc('N', 'V', '1', '2'))) == AV_CODEC_ID_RAWVIDEO);
  CHECK(directShowSubtypeCodec(MEDIASUBTYPE_RGB24) == AV_CODEC_ID_RAWVIDEO);
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
