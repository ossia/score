// An ISF input declared STATIC samples the producer's own texture instead of
// having the video drawn into a render target of its own. A video renderer only
// ever drew its decoder's conversion into its consumers' targets, so the input
// read black; it now converts the frame into a texture of its own for it.
//
// Frames are RGBA8 bytes, red over blue, handed to a fake Video::ExternalInput
// and pushed through a real CameraNode -> VideoNodeRenderer (as in
// GfxVideoScaleFollowsResize.cpp). The STATIC read must match the drawn one.
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/VideoNode.hpp>

#include <Video/ExternalInput.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

#include <cstdint>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr int W = 64;
constexpr int H = 64;

struct RedOverBlueCamera final : Video::ExternalInput
{
  std::vector<uint8_t> bytes;

  RedOverBlueCamera()
  {
    for(int y = 0; y < H; y++)
      for(int x = 0; x < W; x++)
        if(y < H / 2)
          bytes.insert(bytes.end(), {255, 0, 0, 255});
        else
          bytes.insert(bytes.end(), {0, 0, 255, 255});
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

QString writeShader(const QTemporaryDir& dir, const char* name, bool is_static)
{
  const QString path = dir.filePath(QString::fromUtf8(name));
  QFile f(path);
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  f.write(QByteArray(R"(/*{
  "ISFVSN": "2.0",
  "INPUTS": [ { "NAME": "tex", "TYPE": "image")")
          + (is_static ? R"(, "STATIC": true)" : "") + R"( } ]
}*/
void main() { gl_FragColor = IMG_NORM_PIXEL(tex, isf_FragNormCoord); }
)");
  return path;
}

struct Shots
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  ReadbackImage drawn, grabbed;
};

Shots render(score::gfx::GraphicsApi be)
{
  Shots r;
  QTemporaryDir dir;
  if(!dir.isValid())
  {
    r.error = "no temporary directory";
    return r;
  }
  const QString drawnFs = writeShader(dir, "drawn.fs", false);
  const QString staticFs = writeShader(dir, "static.fs", true);
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto cameraNode = std::make_unique<score::gfx::CameraNode>(
        std::make_shared<RedOverBlueCamera>(), QString{});
    cameraNode->setScaleMode(score::gfx::ScaleMode::Stretch);
    const int camera = p.addNode(std::move(cameraNode));
    const int drawn = p.addIsf(drawnFs);
    const int grabbed = p.addIsf(staticFs);
    if(camera < 0 || drawn < 0 || grabbed < 0)
    {
      r.error = "pipeline build failed: " + p.error();
      return;
    }
    const int drawnSink = p.addSink({W, H});
    const int grabbedSink = p.addSink({W, H});
    p.wire(p.nodeImageOut(camera, 0), p.imageIn(drawn, 0));
    p.wire(p.nodeImageOut(camera, 0), p.imageIn(grabbed, 0));
    p.wire(p.imageOut(drawn, 0), p.sinkInput(drawnSink));
    p.wire(p.imageOut(grabbed, 0), p.sinkInput(grabbedSink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();
    p.render(6);
    r.drawn = p.readback(drawnSink);
    r.grabbed = p.readback(grabbedSink);
    if(r.error.empty())
      r.error = p.error();
  });
  return r;
}
}

TEST_CASE("A STATIC image input fed by a video reads the frame", "[gfx][video][static]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shots s = render(backend);
  if(s.skipped)
    SKIP(s.backend + ": " + s.skip_reason);
  CAPTURE(s.backend);
  INFO("error=" << s.error);
  REQUIRE(s.error.empty());
  REQUIRE(s.drawn.valid());
  REQUIRE(s.grabbed.valid());

  for(int y : {H / 4, 3 * H / 4})
  {
    const auto d = s.drawn.at(W / 2, y);
    const auto g = s.grabbed.at(W / 2, y);
    INFO(
        "y=" << y << " drawn " << (int)d[0] << "," << (int)d[1] << "," << (int)d[2]
             << " static " << (int)g[0] << "," << (int)g[1] << "," << (int)g[2]);
    CHECK((near(d, {255, 0, 0, 255}, 8) || near(d, {0, 0, 255, 255}, 8)));
    CHECK(near(g, d, 8));
  }
}
