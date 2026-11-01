// Previews read back by the render list of an output of the patch (PreviewTap):
// what they read, in which orientation, and which lists host them.
//
//     ctest -R gfx_preview_tap --output-on-failure

#include <Gfx/Graph/PreviewTap.hpp>

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

using score::test::gfx::ReadbackImage;
using score::test::gfx::near;

ReadbackImage image(const QRhiReadbackResult& rb)
{
  ReadbackImage img;
  img.width = rb.pixelSize.width();
  img.height = rb.pixelSize.height();
  img.bytes = rb.data;
  return img;
}

struct TapRun
{
  bool skipped{};
  std::string skip_reason;
  std::string error;
  //! What the output read back, and what the tap did
  ReadbackImage output;
  ReadbackImage tap;
  int64_t tapFrames{};
  bool readable{};
};

struct TapSetup
{
  QSize outputSize{64, 64};
  QSize tapSize{16, 16};
  bool active{true};
  //! The output is one of the editor's previews
  bool previewOutput{};
  //! Drops the tap after this many frames, -1: never
  int removeAfter{-1};
  int frames{4};
};

// `shader` renders into an offscreen output of the patch; a tap on it reads
// its output port.
TapRun run(score::gfx::GraphicsApi api, const char* shader, TapSetup setup)
{
  TapRun r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(api == score::gfx::Null)
    {
      r.skipped = true;
      r.skip_reason = score::test::gfx::null_backend_skip_reason();
      return;
    }
    std::string probed;
    if(!score::test::gfx::probe_api(api, probed))
    {
      r.skipped = true;
      r.skip_reason = std::string(score::test::gfx::backend_name(api)) + " unavailable";
      return;
    }

    auto built = score::test::gfx::make_isf_node(corpus(shader));
    if(!built.node)
    {
      r.error = built.error;
      return;
    }
    auto& node = *built.node;
    auto out = std::make_unique<score::gfx::BackgroundNode>();
    out->shared_readback = std::make_shared<QRhiReadbackResult>();
    out->setSize(setup.outputSize);
    out->preview = setup.previewOutput;

    auto tap = std::make_shared<score::gfx::PreviewTap>();
    tap->port = 0;
    tap->size = setup.tapSize;
    tap->active = setup.active;
    tap->readback = std::make_shared<QRhiReadbackResult>();
    node.previewTaps.push_back(tap);

    {
      score::gfx::Graph graph;
      node.nodeId = 1;
      out->nodeId = 2;
      graph.addNode(&node);
      graph.addNode(out.get());
      graph.addEdge(node.output[0], out->input[0], Process::CableType::ImmediateGlutton);
      graph.createAllRenderLists(api);
      if(!out->renderState())
      {
        r.skipped = true;
        r.skip_reason = "no offscreen render state";
        return;
      }

      for(int f = 0; f < setup.frames; f++)
      {
        if(f == setup.removeAfter)
        {
          // The list releases what it drew the tap with once nothing holds it
          r.tapFrames = tap->frames;
          node.previewTaps.clear();
          tap.reset();
        }
        score::test::gfx::pump_frame({&node}, {out.get()}, f, setup.frames);
      }
      // The readbacks of the last frame complete with the next one
      out->renderState()->rhi->finish();

      r.output = image(*out->shared_readback);
      if(tap)
      {
        r.tap = image(*tap->readback);
        r.tapFrames = tap->frames;
        r.readable = tap->readable;
      }
    }
    node.previewTaps.clear();
  });
  return r;
}
}

TEST_CASE("a tap reads its node in the render list of the output", "[gfx][preview_tap]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));
  const auto r = run(api, "isf-solid-color.fs", {});
  if(r.skipped)
    SKIP(r.skip_reason);
  REQUIRE(r.error.empty());
  CHECK(r.readable);
  CHECK(r.tapFrames > 0);
  REQUIRE(r.tap.valid());
  // Fitted into the size asked, with the aspect of the 64x64 texture
  CHECK(r.tap.width == 16);
  CHECK(r.tap.height == 16);
  const std::array<uint8_t, 4> magenta{255, 0, 255, 255};
  CHECK(near(r.tap.center(), magenta, 2));
}

TEST_CASE("a tap keeps the aspect of the texture", "[gfx][preview_tap]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));
  const auto r = run(api, "isf-solid-color.fs", {.outputSize = {128, 64}, .tapSize = {32, 32}});
  if(r.skipped)
    SKIP(r.skip_reason);
  REQUIRE(r.error.empty());
  REQUIRE(r.tap.valid());
  CHECK(r.tap.width == 32);
  CHECK(r.tap.height == 16);
}

TEST_CASE("a tap shows the image the way the output reads it back", "[gfx][preview_tap]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));
  const auto r = run(api, "isf-orient-quadrants.fs", {.outputSize = {64, 64}, .tapSize = {64, 64}});
  if(r.skipped)
    SKIP(r.skip_reason);
  REQUIRE(r.error.empty());
  REQUIRE(r.output.valid());
  REQUIRE(r.tap.valid());
  // Same size: the quadrants of both must match
  const int w = r.tap.width, h = r.tap.height;
  for(auto [x, y] : {std::pair{w / 4, h / 4}, {3 * w / 4, h / 4}, {w / 4, 3 * h / 4}, {3 * w / 4, 3 * h / 4}})
  {
    CAPTURE(x, y);
    CHECK(near(r.tap.at(x, y), r.output.at(x, y), 3));
  }
}

TEST_CASE("an inactive tap reads nothing back", "[gfx][preview_tap]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));
  const auto r = run(api, "isf-solid-color.fs", {.active = false});
  if(r.skipped)
    SKIP(r.skip_reason);
  REQUIRE(r.error.empty());
  // Whether it could is still known: the preview picks its path from it
  CHECK(r.readable);
  CHECK(r.tapFrames == 0);
  CHECK_FALSE(r.tap.valid());
}

TEST_CASE("the editor's previews do not host taps", "[gfx][preview_tap]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));
  const auto r = run(api, "isf-solid-color.fs", {.previewOutput = true});
  if(r.skipped)
    SKIP(r.skip_reason);
  REQUIRE(r.error.empty());
  CHECK(r.tapFrames == 0);
}

TEST_CASE("a list keeps rendering once a tap goes away", "[gfx][preview_tap]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));
  const auto r = run(api, "isf-solid-color.fs", {.removeAfter = 2, .frames = 5});
  if(r.skipped)
    SKIP(r.skip_reason);
  REQUIRE(r.error.empty());
  CHECK(r.tapFrames >= 1);
  CHECK(r.tapFrames <= 2);
  const std::array<uint8_t, 4> magenta{255, 0, 255, 255};
  REQUIRE(r.output.valid());
  CHECK(near(r.output.center(), magenta, 2));
}
