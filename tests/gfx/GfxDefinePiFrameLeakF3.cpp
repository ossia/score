// C6: two regressions that had no test.
//
// A user shader may `#define PI`. When the engine declared `const float PI` in
// the text it concatenates around user code, the define rewrote it into
// `const float 3.1415926535 = ...` and the stage failed to compile. The three
// f3c6-define-pi fixtures (ISF, CSF, raw raster with the define in both stages)
// must compile and paint (sin(PI / 2), PI / 4) = (255, 200).
//
// 92170740b1: an exception thrown while a frame was recording unwound past the
// hand-written endOffscreenFrame(); QRhi kept the frame open and every later
// frame on it silently recorded nothing. RenderList::render is noexcept and
// reports the failure, and OffscreenFrame ends the frame on unwinding. A node
// whose update() throws once must not let the exception out of the render, and
// the frames after it must still render (FRAMEINDEX keeps advancing in the
// readback). An exception unwinding through an OffscreenFrame must leave the
// QRhi out of the frame.
//
// Registration:
//   score_add_gfx_test(define_pi_frame_leak_f3 GfxDefinePiFrameLeakF3.cpp)
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <stdexcept>
#include <string>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

void checkPiColour(const ReadbackImage& img)
{
  REQUIRE(img.valid());
  for(auto [x, y] : std::array<std::array<int, 2>, 3>{{{2, 2}, {8, 8}, {13, 13}}})
  {
    const auto px = img.at(x, y);
    INFO("pixel " << x << "," << y << " = " << int(px[0]) << " " << int(px[1]) << " "
                  << int(px[2]) << " " << int(px[3]));
    CHECK(px[0] >= 250);
    CHECK(px[1] >= 195);
    CHECK(px[1] <= 205);
    CHECK(px[2] <= 5);
  }
}

struct ThrowOnceNode final : score::gfx::ProcessNode
{
  int throwAtUpdate = 3;
  mutable int updates = 0;
  mutable int thrown = 0;

  ThrowOnceNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct ThrowOnceRenderer final : score::gfx::NodeRenderer
{
  const ThrowOnceNode& self;

  explicit ThrowOnceRenderer(const ThrowOnceNode& n)
      : NodeRenderer{n}
      , self{n}
  {
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    m_initialized = true;
  }

  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override
  {
    if(++self.updates == self.throwAtUpdate)
    {
      self.thrown++;
      throw std::runtime_error("ThrowOnceRenderer::update");
    }
  }

  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override { }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList&) override { m_initialized = false; }
};

score::gfx::NodeRenderer* ThrowOnceNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new ThrowOnceRenderer{*this};
}
}

TEST_CASE("a shader's own #define PI compiles and renders", "[gfx][isf][csf][raster]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto file = GENERATE("f3c6-define-pi.fs", "f3c6-define-pi.cs", "f3c6-define-pi-rr");
  CAPTURE(backend_name(api), file);

  const QString f = QString::fromUtf8(file);
  if(f.endsWith(".cs"))
    if(const char* why = compute_shader_skip_reason(api))
      SKIP(why);

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(f.endsWith("-rr"))
    {
      GfxPipeline p;
      const int raster
          = p.addRaster(corpus("f3c6-define-pi-rr.vs"), corpus("f3c6-define-pi-rr.fs"));
      if(raster < 0)
      {
        err = p.error();
        return;
      }
      const int sink = p.addSink({16, 16});
      p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
      if(!p.create(api))
      {
        skipped = p.skipped();
        err = skipped ? std::string{} : p.error();
        return;
      }
      p.render(3);
      img = p.readback(sink);
    }
    else
    {
      auto r = render_isf_chain(api, {corpus(file)}, {16, 16}, 3);
      skipped = r.skipped;
      err = r.error;
      if(!r.outputs.empty())
        img = r.outputs.front();
    }
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err);
  REQUIRE(err.empty());
  checkPiColour(img);
}

TEST_CASE("a render that throws does not leave the frame open", "[gfx][render]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  int escaped = 0;
  int thrown = 0;
  int redBeforeThrow = -1;
  int redAfter = -1;
  int greenAfter = -1;
  bool recordingAfterUnwind = true;
  bool frameAfterUnwind = false;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto node = std::make_unique<ThrowOnceNode>();
    auto* thrower = node.get();
    const int src = p.addNode(std::move(node));
    const int isf = p.addIsf(corpus("f3c6-frameindex-input.fs"));
    if(src < 0 || isf < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({16, 16});
    p.wire(p.nodeImageOut(src, 0), p.imageIn(isf, 0));
    p.wire(p.imageOut(isf, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }

    auto frame = [&] {
      try
      {
        p.render(1);
      }
      catch(...)
      {
        escaped++;
      }
    };

    for(int i = 0; i < 20 && thrower->updates < thrower->throwAtUpdate - 1; i++)
      frame();
    if(auto img = p.readback(sink); img.valid())
      redBeforeThrow = img.at(8, 8)[0];

    for(int i = 0; i < 8; i++)
      frame();
    thrown = thrower->thrown;
    if(auto img = p.readback(sink); img.valid())
    {
      redAfter = img.at(8, 8)[0];
      greenAfter = img.at(8, 8)[1];
    }

    auto st = p.sink(sink)->renderState();
    if(!st || !st->rhi)
    {
      err = "no render state";
      return;
    }
    QRhi& rhi = *st->rhi;
    try
    {
      score::gfx::OffscreenFrame f{rhi};
      if(f)
        throw std::runtime_error("unwinding through an OffscreenFrame");
    }
    catch(const std::runtime_error&)
    {
    }
    recordingAfterUnwind = rhi.isRecordingFrame();
    {
      score::gfx::OffscreenFrame f{rhi};
      frameAfterUnwind = bool(f);
    }
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err << " red before " << redBeforeThrow << ", after " << redAfter);
  REQUIRE(err.empty());
  CHECK(thrown == 1);
  CHECK(escaped == 0);
  CHECK(greenAfter >= 250);
  REQUIRE(redBeforeThrow >= 0);
  CHECK(redAfter >= redBeforeThrow + 6);
  CHECK_FALSE(recordingAfterUnwind);
  CHECK(frameAfterUnwind);
}
