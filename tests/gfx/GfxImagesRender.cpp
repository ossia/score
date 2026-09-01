// Render-level tests for the Images process (score::gfx::ImagesNode).
//
// GfxImagesProcess.cpp covers the model; nothing asserted pixels. Yet the
// classic field failure of this process is exactly a pixel-level one: hit
// play and the image does not come up, then clicking a preview viewport
// (which rebuilds the render lists) suddenly makes it appear on the main
// output. That is a bug of the INCREMENTAL path: the renderer is initialized
// before the exec thread has delivered the image list, and everything that
// follows — texture creation, upload, SRB rewrite — happens through
// update()/Message processing on a live renderer. A full rebuild hides the
// bug because initState() then runs with the images already present.
//
// So every scenario here permutes WHEN the image list reaches the node
// relative to renderer creation and rendering, and asserts the actual
// readback, on every available RHI backend.

#include "GfxProcessDoc.hpp"

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/ImageNode.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString write_png(const QString& dir, const char* name, QRgb color, QSize sz = {16, 16})
{
  QImage img{sz, QImage::Format_ARGB32};
  img.fill(color);
  const QString path = dir + "/" + QString::fromUtf8(name);
  REQUIRE(img.save(path, "PNG"));
  return path;
}

ossia::value paths_value(const std::vector<QString>& paths)
{
  std::vector<ossia::value> v;
  for(auto& p : paths)
    v.push_back(p.toStdString());
  return v;
}

// The Message layout the Images executor produces: one slot per inlet, in
// inlet order. Slots left as monostate are ignored by ImagesNode::process.
enum ImagesPort
{
  PortIndex = 0,
  PortOpacity = 1,
  PortPosition = 2,
  PortScaleX = 3,
  PortScaleY = 4,
  PortImages = 5,
  PortTile = 6,
  PortScaleMode = 7,
  ImagesPortCount = 8
};

score::gfx::Message make_msg(int32_t node_id)
{
  score::gfx::Message m;
  m.node_id = node_id;
  m.input.resize(ImagesPortCount);
  return m;
}

// What the executor delivers on the first tick of playback: every control's
// current value. Position (0,0) is the centered default the inlet carries;
// the UBO's own default is an off-center (0.5, 0.5) until this arrives.
score::gfx::Message initial_controls(int32_t node_id, const ossia::value& images)
{
  auto m = make_msg(node_id);
  m.input[PortIndex] = ossia::value{0};
  m.input[PortOpacity] = ossia::value{1.f};
  m.input[PortPosition] = ossia::value{ossia::vec2f{0.f, 0.f}};
  m.input[PortScaleX] = ossia::value{1.f};
  m.input[PortScaleY] = ossia::value{1.f};
  m.input[PortImages] = images;
  m.input[PortTile] = ossia::value{0};
  m.input[PortScaleMode] = ossia::value{(int)score::gfx::ScaleMode::Original};
  return m;
}

bool is_color(std::array<uint8_t, 4> px, QRgb color, int tol = 8)
{
  return near(
      px, {uint8_t(qRed(color)), uint8_t(qGreen(color)), uint8_t(qBlue(color)), 255},
      tol);
}

struct Colors
{
  static constexpr QRgb red = qRgb(255, 0, 0);
  static constexpr QRgb green = qRgb(0, 255, 0);
  static constexpr QRgb blue = qRgb(0, 0, 255);
};
}

TEST_CASE("Images render when the list is set before render-list creation", "[gfx][images][render][gui]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  run_in_gui_app([backend](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc);
    const QString dir = gfxproc::scratch_dir("images-render-early");
    const auto red = write_png(dir, "red.png", Colors::red);

    GfxPipeline p;
    const int img = p.addNode(std::make_unique<score::gfx::ImagesNode>(doc->context()));
    const int sink = p.addSink({64, 64});
    p.wire(p.nodeImageOut(img), p.sinkInput(sink));

    // Delivered before create(): initState sees the images.
    p.node(img)->process(initial_controls(p.node(img)->nodeId, paths_value({red})));

    if(!p.create(backend))
    {
      WARN(p.backend() << ": " << p.skipReason());
      return;
    }
    REQUIRE(p.error().empty());
    p.render(3);

    const auto rb = p.readback(sink);
    REQUIRE(rb.valid());
    INFO("center: " << (int)rb.center()[0] << "," << (int)rb.center()[1] << ","
                    << (int)rb.center()[2]);
    CHECK(is_color(rb.center(), Colors::red));
  });
}

TEST_CASE("Images render when the list arrives only after the first frames", "[gfx][images][render][gui]")
{
  // The play-button scenario: the renderer is created and renders a few
  // frames before the exec thread's first Message lands. The image must
  // still come up without any render-list rebuild.
  const auto backend = GENERATE(from_range(platform_backends()));
  run_in_gui_app([backend](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc);
    const QString dir = gfxproc::scratch_dir("images-render-late");
    const auto red = write_png(dir, "red.png", Colors::red);

    GfxPipeline p;
    const int img = p.addNode(std::make_unique<score::gfx::ImagesNode>(doc->context()));
    const int sink = p.addSink({64, 64});
    p.wire(p.nodeImageOut(img), p.sinkInput(sink));

    if(!p.create(backend))
    {
      WARN(p.backend() << ": " << p.skipReason());
      return;
    }
    REQUIRE(p.error().empty());

    // Frames with no image at all: must render, and render nothing.
    p.render(2);
    {
      const auto rb = p.readback(sink);
      REQUIRE(rb.valid());
      CHECK_FALSE(is_color(rb.center(), Colors::red));
    }

    // Now the exec thread catches up.
    p.node(img)->process(initial_controls(p.node(img)->nodeId, paths_value({red})));
    p.render(3);

    const auto rb = p.readback(sink);
    REQUIRE(rb.valid());
    INFO("center: " << (int)rb.center()[0] << "," << (int)rb.center()[1] << ","
                    << (int)rb.center()[2]);
    CHECK(is_color(rb.center(), Colors::red));
  });
}

TEST_CASE("A sink added after a late image list shows the image too", "[gfx][images][render][gui]")
{
  // The "clicking the preview viewport makes it appear" path, as a positive
  // requirement: adding a second output must show the image on BOTH.
  const auto backend = GENERATE(from_range(platform_backends()));
  run_in_gui_app([backend](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc);
    const QString dir = gfxproc::scratch_dir("images-render-preview");
    const auto red = write_png(dir, "red.png", Colors::red);

    GfxPipeline p;
    const int img = p.addNode(std::make_unique<score::gfx::ImagesNode>(doc->context()));
    const int sink = p.addSink({64, 64});
    const int preview = p.addSink({64, 64});
    p.wire(p.nodeImageOut(img), p.sinkInput(sink));

    if(!p.create(backend))
    {
      WARN(p.backend() << ": " << p.skipReason());
      return;
    }
    REQUIRE(p.error().empty());
    p.render(2);

    p.node(img)->process(initial_controls(p.node(img)->nodeId, paths_value({red})));
    p.render(2);

    // "Click on the preview": wire the second output through the same
    // incremental path the app uses for a new cable.
    p.addEdgeIncremental(p.nodeImageOut(img), p.sinkInput(preview));
    p.render(3);

    const auto main_rb = p.readback(sink);
    const auto prev_rb = p.readback(preview);
    REQUIRE(main_rb.valid());
    REQUIRE(prev_rb.valid());
    CHECK(is_color(main_rb.center(), Colors::red));
    CHECK(is_color(prev_rb.center(), Colors::red));
  });
}

TEST_CASE("The index control switches between images", "[gfx][images][render][gui]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  run_in_gui_app([backend](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc);
    const QString dir = gfxproc::scratch_dir("images-render-index");
    const auto red = write_png(dir, "red.png", Colors::red);
    const auto green = write_png(dir, "green.png", Colors::green);

    GfxPipeline p;
    const int img = p.addNode(std::make_unique<score::gfx::ImagesNode>(doc->context()));
    const int sink = p.addSink({64, 64});
    p.wire(p.nodeImageOut(img), p.sinkInput(sink));

    p.node(img)->process(
        initial_controls(p.node(img)->nodeId, paths_value({red, green})));

    if(!p.create(backend))
    {
      WARN(p.backend() << ": " << p.skipReason());
      return;
    }
    REQUIRE(p.error().empty());
    p.render(3);
    CHECK(is_color(p.readback(sink).center(), Colors::red));

    auto m = make_msg(p.node(img)->nodeId);
    m.input[PortIndex] = ossia::value{1};
    p.node(img)->process(std::move(m));
    p.render(2);
    CHECK(is_color(p.readback(sink).center(), Colors::green));

    // Out-of-range indices wrap rather than go blank.
    auto m2 = make_msg(p.node(img)->nodeId);
    m2.input[PortIndex] = ossia::value{2};
    p.node(img)->process(std::move(m2));
    p.render(2);
    CHECK(is_color(p.readback(sink).center(), Colors::red));

    auto m3 = make_msg(p.node(img)->nodeId);
    m3.input[PortIndex] = ossia::value{-1};
    p.node(img)->process(std::move(m3));
    p.render(2);
    CHECK(is_color(p.readback(sink).center(), Colors::green));
  });
}

TEST_CASE("Replacing the list mid-playback switches content and survives resize", "[gfx][images][render][gui]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  run_in_gui_app([backend](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc);
    const QString dir = gfxproc::scratch_dir("images-render-replace");
    const auto red = write_png(dir, "red.png", Colors::red, {16, 16});
    // Different size on purpose: exercises the texture-resize branch of
    // recreateTextures on a live renderer.
    const auto blue = write_png(dir, "blue.png", Colors::blue, {48, 24});

    GfxPipeline p;
    const int img = p.addNode(std::make_unique<score::gfx::ImagesNode>(doc->context()));
    const int sink = p.addSink({64, 64});
    p.wire(p.nodeImageOut(img), p.sinkInput(sink));

    p.node(img)->process(initial_controls(p.node(img)->nodeId, paths_value({red})));
    if(!p.create(backend))
    {
      WARN(p.backend() << ": " << p.skipReason());
      return;
    }
    REQUIRE(p.error().empty());
    p.render(3);
    CHECK(is_color(p.readback(sink).center(), Colors::red));

    auto m = make_msg(p.node(img)->nodeId);
    m.input[PortImages] = paths_value({blue});
    p.node(img)->process(std::move(m));
    p.render(3);
    CHECK(is_color(p.readback(sink).center(), Colors::blue));

    // Emptying the list must go back to showing nothing, not keep a stale
    // texture and not crash.
    auto m2 = make_msg(p.node(img)->nodeId);
    m2.input[PortImages] = paths_value({});
    p.node(img)->process(std::move(m2));
    p.render(3);
    CHECK_FALSE(is_color(p.readback(sink).center(), Colors::blue));
    CHECK_FALSE(is_color(p.readback(sink).center(), Colors::red));
  });
}

TEST_CASE("Opacity and tile-mode changes keep the image visible", "[gfx][images][render][gui]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  run_in_gui_app([backend](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc);
    const QString dir = gfxproc::scratch_dir("images-render-modes");
    const auto red = write_png(dir, "red.png", Colors::red);

    GfxPipeline p;
    const int img = p.addNode(std::make_unique<score::gfx::ImagesNode>(doc->context()));
    const int sink = p.addSink({64, 64});
    p.wire(p.nodeImageOut(img), p.sinkInput(sink));

    p.node(img)->process(initial_controls(p.node(img)->nodeId, paths_value({red})));
    if(!p.create(backend))
    {
      WARN(p.backend() << ": " << p.skipReason());
      return;
    }
    REQUIRE(p.error().empty());
    p.render(3);
    REQUIRE(is_color(p.readback(sink).center(), Colors::red));

    // Opacity 0: image contributes nothing.
    {
      auto m = make_msg(p.node(img)->nodeId);
      m.input[PortOpacity] = ossia::value{0.f};
      p.node(img)->process(std::move(m));
      p.render(2);
      CHECK_FALSE(is_color(p.readback(sink).center(), Colors::red));
    }
    // And back.
    {
      auto m = make_msg(p.node(img)->nodeId);
      m.input[PortOpacity] = ossia::value{1.f};
      p.node(img)->process(std::move(m));
      p.render(2);
      CHECK(is_color(p.readback(sink).center(), Colors::red));
    }

    // Tile-mode change swaps the sampler on a live renderer; the texture
    // binding must survive it. (Tiled mode fills the target, so the center
    // stays red.)
    {
      auto m = make_msg(p.node(img)->nodeId);
      m.input[PortTile] = ossia::value{(int)score::gfx::Tiled};
      p.node(img)->process(std::move(m));
      p.render(3);
      CHECK(is_color(p.readback(sink).center(), Colors::red));
    }
    // And back to single.
    {
      auto m = make_msg(p.node(img)->nodeId);
      m.input[PortTile] = ossia::value{(int)score::gfx::Single};
      p.node(img)->process(std::move(m));
      p.render(3);
      CHECK(is_color(p.readback(sink).center(), Colors::red));
    }
  });
}
