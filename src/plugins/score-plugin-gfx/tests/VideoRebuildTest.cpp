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
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

#include <cstdio>

namespace
{
int g_fail = 0;

void check(bool ok, const char* what)
{
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok)
    ++g_fail;
}

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
  return f;  // height stays the PICTURE height: 1080
}

// The rebuild rule is a free function: a static member of the renderer would
// not be exported from the plugin, and this test has to link against it.
constexpr auto R_needsRebuild = &score::gfx::videoDecoderNeedsRebuild;
}

int main()
{
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::printf("\n== the steady state ==\n");
  {
    const auto f = progressive1080();
    check(
        R_needsRebuild(false, f, f, f.pixel_format, 1920, 1080),
        "with no decoder yet, always rebuild");
    check(
        !R_needsRebuild(true, f, f, f.pixel_format, 1920, 1080),
        "an unchanged progressive frame does not rebuild");
  }

  // The one this file exists for.
  std::printf("\n== a fielded source, frame after frame ==\n");
  {
    const auto f = fielded1080();
    check(
        !R_needsRebuild(true, f, f, f.pixel_format, 1920, 540),
        "a 540-line field against a 1080-line picture does NOT rebuild");
    check(
        R_needsRebuild(true, f, f, f.pixel_format, 1920, 1080),
        "a full-height frame on a fielded stream DOES rebuild");

    // And the trap in the other direction: without the doubling, a progressive
    // 540-line source would be mistaken for a field.
    auto prog540 = progressive1080();
    prog540.height = 540;
    check(
        !R_needsRebuild(true, prog540, prog540, prog540.pixel_format, 1920, 540),
        "a genuinely 540-line progressive source does not rebuild either");
  }

  std::printf("\n== what must rebuild ==\n");
  {
    const auto built = progressive1080();

    auto size = built;
    check(
        R_needsRebuild(true, built, size, built.pixel_format, 1280, 720),
        "a size change rebuilds");
    check(
        R_needsRebuild(true, built, size, AV_PIX_FMT_NV12, 1920, 1080),
        "a pixel format change rebuilds");

    auto colour = built;
    colour.color_space = AVCOL_SPC_SMPTE170M;
    check(
        R_needsRebuild(true, built, colour, built.pixel_format, 1920, 1080),
        "a colour space change rebuilds -- the matrix is baked into the shader");

    auto range = built;
    range.color_range = AVCOL_RANGE_JPEG;
    check(
        R_needsRebuild(true, built, range, built.pixel_format, 1920, 1080),
        "a colour range change rebuilds");

    auto trc = built;
    trc.color_trc = AVCOL_TRC_ARIB_STD_B67;
    check(
        R_needsRebuild(true, built, trc, built.pixel_format, 1920, 1080),
        "a transfer change rebuilds -- an SDR source turning HDR mid-stream");

    auto pri = built;
    pri.color_primaries = AVCOL_PRI_BT2020;
    check(
        R_needsRebuild(true, built, pri, built.pixel_format, 1920, 1080),
        "a primaries change rebuilds");

    auto interlaced = built;
    interlaced.interlacing = Video::Interlacing::Fields;
    check(
        R_needsRebuild(true, built, interlaced, built.pixel_format, 1920, 1080),
        "starting to deliver fields rebuilds -- the texture geometry changes");

    auto out = built;
    out.output_format = Video::OutputFormat::Passthrough;
    check(
        R_needsRebuild(true, built, out, built.pixel_format, 1920, 1080),
        "an output format change rebuilds");

    auto tm = built;
    tm.tonemap = Video::Tonemap::Hable;
    check(
        R_needsRebuild(true, built, tm, built.pixel_format, 1920, 1080),
        "a tonemap change rebuilds");
  }

  // The deinterlace mode is a uniform in score_tc, not a shader variant, so
  // switching weave to bob must be free.
  std::printf("\n== what must NOT rebuild ==\n");
  {
    const auto built = fielded1080();
    auto bob = built;
    bob.deinterlace = Video::Deinterlace::Bob;
    check(
        !R_needsRebuild(true, built, bob, built.pixel_format, 1920, 540),
        "switching weave to bob does not rebuild: it is a uniform");
  }

  // Where each field's rows land in the stacked texture. A transposition here
  // is not subtle on screen -- the picture interlaces against itself -- but it
  // is cheap to pin and impossible to see in a code review.
  std::printf("\n== field upload geometry ==\n");
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
    check(top.rows == 540 && top.offset == 0, "field 0 fills the TOP half");

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 0, 0)
    f.flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
#else
    f.top_field_first = 0;
#endif
    auto bottom = D::planeRows(fielded, f, 1080);
    check(
        bottom.rows == 540 && bottom.offset == 540, "field 1 fills the BOTTOM half");

    auto whole = D::planeRows(prog, f, 1080);
    check(
        whole.rows == 1080 && whole.offset == 0,
        "a progressive frame fills the whole texture");

    // The chroma plane of a 4:2:0 source has half the rows; the halving has to
    // follow the plane, not the picture.
    auto chroma = D::planeRows(fielded, f, 540);
    check(
        chroma.rows == 270 && chroma.offset == 270,
        "a half-height plane halves again, at its own offset");
  }

  // Which mode the shader runs, including the case that only shows up on a
  // lossy network.
  std::printf("\n== deinterlace mode selection ==\n");
  {
    using score::gfx::videoFieldMode;
    using I = Video::Interlacing;
    using D = Video::Deinterlace;

    check(
        videoFieldMode(I::None, D::Weave, true) == 0.f,
        "a progressive source runs the identity, whatever the setting says");
    check(
        videoFieldMode(I::Woven, D::Bob, false) == 0.f,
        "an already-woven frame is a frame: nothing to do");
    check(
        videoFieldMode(I::Fields, D::Weave, true) == 1.f,
        "fields with a valid partner weave");
    check(
        videoFieldMode(I::Fields, D::Bob, true) == 2.f, "bob when asked for bob");
    check(
        videoFieldMode(I::Fields, D::Weave, false) == 2.f,
        "weave falls back to bob when the partner field is missing");
  }

  std::printf(
      "\nvideo rebuild: %s (%d failure%s)\n", g_fail ? "FAILED" : "passed", g_fail,
      g_fail == 1 ? "" : "s");
  return g_fail ? 1 : 0;
}
