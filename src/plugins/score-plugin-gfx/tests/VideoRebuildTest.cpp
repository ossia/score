// VideoNodeRenderer::needsRebuild -- when the GPU decoder has to be thrown away
// and rebuilt.
//
// This is tested on its own because a wrong answer here does not look like a
// bug. Rebuilding on every frame is not a visual defect, it is a frame rate
// that is quietly worse than it should be, and the most likely way to get
// there is the fielded case: a source that describes a 1080-line picture while
// handing over 540-line fields looks like a size change on every single frame
// unless the two heights are compared in the same units.
//
// No GPU and no video file: the function is pure.

#include <Gfx/Graph/VideoNodeRenderer.hpp>
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
Video::ImageFormat progressive1080()
{
  Video::ImageFormat f;
  f.width = 1920;
  f.height = 1080;
  f.pixel_format = AV_PIX_FMT_YUV420P;
  f.color_space = AVCOL_SPC_BT709;
  f.color_range = AVCOL_RANGE_MPEG;
  f.color_trc = AVCOL_TRC_BT709;
  f.color_primaries = AVCOL_PRI_BT709;
  f.interlacing = Video::Interlacing::None;
  f.output_format = Video::OutputFormat::SDR;
  f.tonemap = Video::Tonemap::Auto;
  f.deinterlace = Video::Deinterlace::Weave;
  return f;
}

Video::ImageFormat fielded1080()
{
  auto f = progressive1080();
  f.pixel_format = AV_PIX_FMT_P216LE;
  f.interlacing = Video::Interlacing::Fields;
  return f; // height stays the PICTURE height: 1080
}

// The rebuild rule is a free function: a static member of the renderer would
// not be exported from the plugin, and this test has to link against it.
constexpr auto R_needsRebuild = &score::gfx::videoDecoderNeedsRebuild;
}

TEST_CASE("the steady state", "[gfx][video]")
{
  const auto f = progressive1080();
  SECTION("with no decoder yet, always rebuild")
  {
    CHECK(R_needsRebuild(false, f, f, f.pixel_format, 1920, 1080));
  }
  SECTION("an unchanged progressive frame does not rebuild")
  {
    CHECK(!R_needsRebuild(true, f, f, f.pixel_format, 1920, 1080));
  }
}

// The one this file exists for.
TEST_CASE("a fielded source, frame after frame", "[gfx][video]")
{
  const auto f = fielded1080();
  SECTION("a 540-line field against a 1080-line picture does NOT rebuild")
  {
    CHECK(!R_needsRebuild(true, f, f, f.pixel_format, 1920, 540));
  }
  SECTION("a full-height frame on a fielded stream DOES rebuild")
  {
    CHECK(R_needsRebuild(true, f, f, f.pixel_format, 1920, 1080));
  }

  // And the trap in the other direction: without the doubling, a progressive
  // 540-line source would be mistaken for a field.
  auto prog540 = progressive1080();
  prog540.height = 540;
  SECTION("a genuinely 540-line progressive source does not rebuild either")
  {
    CHECK(!R_needsRebuild(true, prog540, prog540, prog540.pixel_format, 1920, 540));
  }
}

TEST_CASE("what must rebuild", "[gfx][video]")
{
  const auto built = progressive1080();

  auto size = built;
  SECTION("a size change rebuilds")
  {
    CHECK(R_needsRebuild(true, built, size, built.pixel_format, 1280, 720));
  }
  SECTION("a pixel format change rebuilds")
  {
    CHECK(R_needsRebuild(true, built, size, AV_PIX_FMT_NV12, 1920, 1080));
  }

  auto colour = built;
  colour.color_space = AVCOL_SPC_SMPTE170M;
  SECTION("a colour space change rebuilds -- the matrix is baked into the shader")
  {
    CHECK(R_needsRebuild(true, built, colour, built.pixel_format, 1920, 1080));
  }

  auto range = built;
  range.color_range = AVCOL_RANGE_JPEG;
  SECTION("a colour range change rebuilds")
  {
    CHECK(R_needsRebuild(true, built, range, built.pixel_format, 1920, 1080));
  }

  auto trc = built;
  trc.color_trc = AVCOL_TRC_ARIB_STD_B67;
  SECTION("a transfer change rebuilds -- an SDR source turning HDR mid-stream")
  {
    CHECK(R_needsRebuild(true, built, trc, built.pixel_format, 1920, 1080));
  }

  auto pri = built;
  pri.color_primaries = AVCOL_PRI_BT2020;
  SECTION("a primaries change rebuilds")
  {
    CHECK(R_needsRebuild(true, built, pri, built.pixel_format, 1920, 1080));
  }

  auto interlaced = built;
  interlaced.interlacing = Video::Interlacing::Fields;
  SECTION("starting to deliver fields rebuilds -- the texture geometry changes")
  {
    CHECK(R_needsRebuild(true, built, interlaced, built.pixel_format, 1920, 1080));
  }

  auto out = built;
  out.output_format = Video::OutputFormat::Passthrough;
  SECTION("an output format change rebuilds")
  {
    CHECK(R_needsRebuild(true, built, out, built.pixel_format, 1920, 1080));
  }

  auto tm = built;
  tm.tonemap = Video::Tonemap::Hable;
  SECTION("a tonemap change rebuilds")
  {
    CHECK(R_needsRebuild(true, built, tm, built.pixel_format, 1920, 1080));
  }
}

// The deinterlace mode is a uniform in score_tc, not a shader variant, so
// switching weave to bob must be free.
TEST_CASE("what must NOT rebuild", "[gfx][video]")
{
  const auto built = fielded1080();
  auto bob = built;
  bob.deinterlace = Video::Deinterlace::Bob;
  SECTION("switching weave to bob does not rebuild: it is a uniform")
  {
    CHECK(!R_needsRebuild(true, built, bob, built.pixel_format, 1920, 540));
  }
}

// Where each field's rows land in the stacked texture. A transposition here
// is not subtle on screen -- the picture interlaces against itself -- but it
// is cheap to pin and impossible to see in a code review.
TEST_CASE("field upload geometry", "[gfx][video]")
{
  using D = score::gfx::GPUVideoDecoder;
  AVFrame f{};
  const auto fielded = fielded1080();
  const auto prog = progressive1080();

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 0, 0)
  f.flags |= AV_FRAME_FLAG_TOP_FIELD_FIRST;
#else
  f.top_field_first = 1;
#endif
  auto top = D::planeRows(fielded, f, 1080);
  SECTION("field 0 fills the TOP half")
  {
    CHECK(top.rows == 540);
    CHECK(top.offset == 0);
  }

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 0, 0)
  f.flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
#else
  f.top_field_first = 0;
#endif
  auto bottom = D::planeRows(fielded, f, 1080);
  SECTION("field 1 fills the BOTTOM half")
  {
    CHECK(bottom.rows == 540);
    CHECK(bottom.offset == 540);
  }

  auto whole = D::planeRows(prog, f, 1080);
  SECTION("a progressive frame fills the whole texture")
  {
    CHECK(whole.rows == 1080);
    CHECK(whole.offset == 0);
  }

  // The chroma plane of a 4:2:0 source has half the rows; the halving has to
  // follow the plane, not the picture.
  auto chroma = D::planeRows(fielded, f, 540);
  SECTION("a half-height plane halves again, at its own offset")
  {
    CHECK(chroma.rows == 270);
    CHECK(chroma.offset == 270);
  }
}

// Which mode the shader runs, including the case that only shows up on a
// lossy network.
TEST_CASE("deinterlace mode selection", "[gfx][video]")
{
  using score::gfx::videoFieldMode;
  using I = Video::Interlacing;
  using D = Video::Deinterlace;

  SECTION("a progressive source runs the identity, whatever the setting says")
  {
    CHECK(videoFieldMode(I::None, D::Weave, true) == 0.f);
  }
  SECTION("an already-woven frame is a frame: nothing to do")
  {
    CHECK(videoFieldMode(I::Woven, D::Bob, false) == 0.f);
  }
  SECTION("fields with a valid partner weave")
  {
    CHECK(videoFieldMode(I::Fields, D::Weave, true) == 1.f);
  }
  SECTION("bob when asked for bob")
  {
    CHECK(videoFieldMode(I::Fields, D::Bob, true) == 2.f);
  }
  SECTION("weave falls back to bob when the partner field is missing")
  {
    CHECK(videoFieldMode(I::Fields, D::Weave, false) == 2.f);
  }
}

TEST_CASE("a frame completes the colour its source does not declare", "[gfx][video]")
{
  using namespace score::gfx;
  constexpr auto U = AVCOL_RANGE_UNSPECIFIED;
  constexpr auto J = AVCOL_RANGE_JPEG;
  constexpr auto M = AVCOL_RANGE_MPEG;
  SECTION("a YUVJ frame from a source that declares no range is full range")
  {
    CHECK(effectiveColorRange(AV_PIX_FMT_YUVJ422P, U, U) == J);
  }
  SECTION("a frame that says full range is full range when the source says nothing")
  {
    CHECK(effectiveColorRange(AV_PIX_FMT_YUV422P, U, J) == J);
  }
  SECTION("what the source declares wins")
  {
    CHECK(effectiveColorRange(AV_PIX_FMT_YUVJ422P, M, J) == M);
  }
  SECTION("a plain YUV format with no range anywhere stays unspecified")
  {
    CHECK(effectiveColorRange(AV_PIX_FMT_YUV420P, U, U) == U);
  }

  constexpr auto SU = AVCOL_SPC_UNSPECIFIED;
  SECTION("a YUVJ frame with no matrix anywhere is BT.601, as JFIF says")
  {
    CHECK(effectiveColorSpace(AV_PIX_FMT_YUVJ422P, SU, SU) == AVCOL_SPC_BT470BG);
  }
  SECTION("a frame's own matrix is used when the source declares none")
  {
    CHECK(
        effectiveColorSpace(AV_PIX_FMT_YUV422P, SU, AVCOL_SPC_BT470BG)
        == AVCOL_SPC_BT470BG);
  }
  SECTION("the matrix the source declares wins")
  {
    CHECK(
        effectiveColorSpace(AV_PIX_FMT_YUVJ422P, AVCOL_SPC_BT709, AVCOL_SPC_BT470BG)
        == AVCOL_SPC_BT709);
  }
  SECTION("a plain YUV format with no matrix anywhere stays unspecified")
  {
    CHECK(effectiveColorSpace(AV_PIX_FMT_YUV420P, SU, SU) == SU);
  }

  Video::ImageFormat camera;
  camera.width = 1280;
  camera.height = 720;
  camera.pixel_format = AV_PIX_FMT_YUVJ422P;
  const auto fmt = AV_PIX_FMT_YUVJ422P;
  const auto sdr = Video::OutputFormat::SDR;
  const auto tm = Video::Tonemap::Auto;
  const FrameColor untagged{};
  const auto first = requestedVideoFormat(camera, fmt, untagged, sdr, tm);
  SECTION("the first camera frame builds the decoder")
  {
    CHECK(videoDecoderNeedsRebuild(false, {}, first, fmt, 1280, 720));
  }
  const auto built = builtVideoFormat({}, first, fmt, 1280, 720);
  SECTION("the decoder is built for full range")
  {
    CHECK(built.color_range == J);
  }
  SECTION("an HD MJPEG frame is converted with full-range BT.601, not BT.709")
  {
    CHECK(colorMatrix(built) == QString(SCORE_GFX_CONVERT_BT601_FULL_TO_RGB));
  }
  SECTION("the next camera frame does not rebuild the decoder")
  {
    CHECK(!videoDecoderNeedsRebuild(
        true, built, requestedVideoFormat(camera, fmt, untagged, sdr, tm), fmt, 1280,
        720));
  }

  Video::ImageFormat stream = camera;
  stream.pixel_format = AV_PIX_FMT_YUV422P;
  const auto yuv = AV_PIX_FMT_YUV422P;
  FrameColor lim;
  lim.range = M;
  FrameColor full;
  full.range = J;
  const auto limited = builtVideoFormat(
      {}, requestedVideoFormat(stream, yuv, lim, sdr, tm), yuv, 1280, 720);
  SECTION("an HD frame that carries no matrix keeps the size-based BT.709")
  {
    CHECK(colorMatrix(limited) == QString(SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB));
  }
  SECTION("a frame whose own range changes rebuilds the decoder")
  {
    CHECK(videoDecoderNeedsRebuild(
        true, limited, requestedVideoFormat(stream, yuv, full, sdr, tm), yuv, 1280,
        720));
  }

  FrameColor tagged;
  tagged.range = J;
  tagged.space = AVCOL_SPC_BT470BG;
  const auto decoded = builtVideoFormat(
      {}, requestedVideoFormat(stream, yuv, tagged, sdr, tm), yuv, 1280, 720);
  SECTION("a frame tagged full-range BT.601 by its decoder is converted as such")
  {
    CHECK(colorMatrix(decoded) == QString(SCORE_GFX_CONVERT_BT601_FULL_TO_RGB));
  }

  const FrameColor hdr{M, AVCOL_SPC_BT2020_NCL, AVCOL_TRC_SMPTE2084, AVCOL_PRI_BT2020};
  const auto pq = requestedVideoFormat(stream, yuv, hdr, sdr, tm);
  SECTION("an input that only tags its frames (AJA staging) keeps its HDR description")
  {
    CHECK(pq.color_trc == AVCOL_TRC_SMPTE2084);
    CHECK(pq.color_primaries == AVCOL_PRI_BT2020);
    CHECK(pq.color_space == AVCOL_SPC_BT2020_NCL);
  }

  Video::ImageFormat sdi = stream;
  sdi.color_range = M;
  sdi.color_space = AVCOL_SPC_BT709;
  sdi.color_trc = AVCOL_TRC_BT709;
  sdi.color_primaries = AVCOL_PRI_BT709;
  const auto kept = requestedVideoFormat(sdi, yuv, hdr, sdr, tm);
  SECTION("an input that declares its colour is not overridden by its frames")
  {
    CHECK(kept.color_trc == AVCOL_TRC_BT709);
    CHECK(kept.color_primaries == AVCOL_PRI_BT709);
    CHECK(kept.color_space == AVCOL_SPC_BT709);
    CHECK(kept.color_range == M);
  }
}
