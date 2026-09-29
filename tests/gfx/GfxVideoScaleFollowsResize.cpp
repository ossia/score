// A video fitted to its target with black bars keeps its aspect ratio when the
// output is resized: the resize re-initialises only what follows the output
// size, and the video renderer's scale follows the target's size on its own.
//
// Frames are RGBA8 bytes handed to a fake Video::ExternalInput and pushed
// through a real CameraNode -> VideoNodeRenderer (as in GfxAlphaVideoFrame.cpp).
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/VideoNode.hpp>

#include <Video/ExternalInput.hpp>

#include <catch2/catch_test_macros.hpp>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

#include <array>
#include <cstdint>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr int W = 64;
constexpr int H = 64;

struct SolidCamera final : Video::ExternalInput
{
  std::vector<uint8_t> bytes;

  SolidCamera()
  {
    for(int i = 0; i < W * H; i++)
      bytes.insert(bytes.end(), {200, 100, 50, 255});
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

struct Shots
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  ReadbackImage before, after;
};

// camera -> sink, square, then resized to twice as wide.
Shots render_resized(score::gfx::GraphicsApi be)
{
  Shots r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto cameraNode
        = std::make_unique<score::gfx::CameraNode>(std::make_shared<SolidCamera>(), QString{});
    cameraNode->setScaleMode(score::gfx::ScaleMode::BlackBars);
    const int camera = p.addNode(std::move(cameraNode));
    const int sink = p.addSink({W, H});
    if(camera < 0 || !p.error().empty())
    {
      r.error = p.error().empty() ? "pipeline build failed" : p.error();
      return;
    }
    p.wire(p.nodeImageOut(camera, 0), p.sinkInput(sink));
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
    r.before = p.readback(sink);
    p.resizeSink(sink, {2 * W, H});
    p.render(4);
    r.after = p.readback(sink);
    if(r.error.empty())
      r.error = p.error();
  });
  return r;
}

bool lit(const ReadbackImage& img, int x, int y)
{
  const auto c = img.at(x, y);
  return c[0] + c[1] + c[2] > 120;
}
}

TEST_CASE(
    "a video with black bars keeps its aspect ratio across an output resize",
    "[gfx][video][resize]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shots s = render_resized(backend);
  if(s.skipped)
    SKIP(s.backend + ": " + s.skip_reason);
  CAPTURE(s.backend);
  INFO("error=" << s.error);
  REQUIRE(s.error.empty());
  REQUIRE(s.before.valid());
  REQUIRE(s.after.valid());
  REQUIRE(s.after.width == 2 * W);

  // The square frame fills the square target, then the middle of the wide one,
  // between two bars.
  CHECK(lit(s.before, 4, H / 2));
  CHECK(lit(s.before, W / 2, H / 2));
  CHECK(lit(s.after, W, H / 2));
  CHECK_FALSE(lit(s.after, 8, H / 2));
  CHECK_FALSE(lit(s.after, 2 * W - 8, H / 2));
}
