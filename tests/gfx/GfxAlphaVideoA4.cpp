// =============================================================================
// N57 follow-up (agent A4): a video frame reaches a straight ISF as the ISF
// reference renderer delivers it.
//
// The video node draws its straight-alpha frame into the consumer's
// premultiplied render target; IMG_THIS_PIXEL gives the ISF straight colour
// back. An opaque frame is exactly what it was before premultiplied targets; a
// translucent frame forwarded by a straight passthrough is stored once, as the
// video node alone stores it.
//
// Frames are RGBA8 bytes handed to a fake Video::ExternalInput and pushed
// through a real CameraNode -> VideoNodeRenderer (as in VideoDecoderPixels.cpp).
// =============================================================================
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/VideoNode.hpp>

#include <Video/ExternalInput.hpp>

#include <catch2/catch_test_macros.hpp>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

#include <cstdint>
#include <vector>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
constexpr int W = 64;
constexpr int H = 64;

struct RgbaCamera final : Video::ExternalInput
{
  std::vector<uint8_t> bytes;

  explicit RgbaCamera(std::array<uint8_t, 4> px)
  {
    for(int i = 0; i < W * H; i++)
      bytes.insert(bytes.end(), px.begin(), px.end());
    this->width = W;
    this->height = H;
    this->pixel_format = AV_PIX_FMT_RGBA;
    this->color_space = AVCOL_SPC_RGB;
    this->color_range = AVCOL_RANGE_JPEG;
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
    f->data[0] = bytes.data();
    f->linesize[0] = W * 4;
    return f;
  }

  void release_frame(AVFrame* frame) noexcept override { av_frame_free(&frame); }
};

struct Shot
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  ReadbackImage image;
};

// camera -> [filter] -> view -> sink
Shot render_video(
    score::gfx::GraphicsApi be, std::array<uint8_t, 4> px, const char* filter)
{
  Shot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto cam = std::make_shared<RgbaCamera>(px);
    const int camera = p.addNode(std::make_unique<score::gfx::CameraNode>(cam, QString{}));
    const int flt = filter ? p.addIsf(corpus(filter)) : -2;
    const int view = p.addIsf(corpus("fixc-opaque-view.fs"));
    const int sink = p.addSink({W, H});
    if(camera < 0 || flt == -1 || view < 0 || !p.error().empty())
    {
      r.error = p.error().empty() ? "pipeline build failed" : p.error();
      return;
    }
    if(flt >= 0)
    {
      p.wire(p.nodeImageOut(camera, 0), p.imageIn(flt, 0));
      p.wire(p.imageOut(flt, 0), p.imageIn(view, 0));
    }
    else
    {
      p.wire(p.nodeImageOut(camera, 0), p.imageIn(view, 0));
    }
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();
    p.render(4);
    r.image = p.readback(sink);
    if(r.error.empty())
      r.error = p.error();
    if(r.error.empty() && !r.image.valid())
      r.error = "empty readback";
  });
  return r;
}

#define A4V_REQUIRE_LIVE(s)                                                  \
  if((s).skipped)                                                            \
    SKIP((s).backend + ": " + (s).skip_reason);                              \
  CAPTURE((s).backend);                                                      \
  REQUIRE((s).error.empty());                                                \
  REQUIRE((s).image.valid())

void check_stored(const Shot& s, std::array<int, 3> rgb, int alpha, int tol)
{
  const auto c = s.image.at(16, 32);
  const auto a = s.image.at(48, 32);
  INFO("stored rgb = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  INFO("stored alpha = " << int(a[0]));
  CHECK(std::abs(int(c[0]) - rgb[0]) <= tol);
  CHECK(std::abs(int(c[1]) - rgb[1]) <= tol);
  CHECK(std::abs(int(c[2]) - rgb[2]) <= tol);
  CHECK(std::abs(int(a[0]) - alpha) <= tol);
}
}

TEST_CASE(
    "A4: an opaque video frame reaches a straight ISF unchanged",
    "[gfx][video][isf][alpha][a4]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot direct = render_video(backend, {64, 160, 200, 255}, nullptr);
  A4V_REQUIRE_LIVE(direct);
  check_stored(direct, {64, 160, 200}, 255, 0);

  const Shot s = render_video(backend, {64, 160, 200, 255}, "a4alpha-straight-pass.fs");
  A4V_REQUIRE_LIVE(s);
  check_stored(s, {64, 160, 200}, 255, 0);
}

TEST_CASE(
    "A4: a translucent video frame forwarded by a straight ISF is stored once",
    "[gfx][video][isf][alpha][a4]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // Straight (200, 100, 50, 128) stores (100, 50, 25, 128).
  const Shot direct = render_video(backend, {200, 100, 50, 128}, nullptr);
  A4V_REQUIRE_LIVE(direct);
  check_stored(direct, {100, 50, 25}, 128, 2);

  const Shot s = render_video(backend, {200, 100, 50, 128}, "a4alpha-straight-pass.fs");
  A4V_REQUIRE_LIVE(s);
  check_stored(s, {100, 50, 25}, 128, 2);
}
