// The Images node's output texture is the same way up as an ISF output on
// every backend.
//
// The source PNG is red over blue. It must come out red on top when the Images
// node draws straight into a sink, through an ISF image input, and mapped onto
// a cube by ModelDisplay by texture coordinates (GfxModelDisplayTexture pins
// the ISF side of that). Under every ModelDisplay projection the Images node
// must give the same picture as an ISF drawing the same red over blue.
//
// Registration: see the test_gfx_images_orientation_fixl target.
#include "GfxProcessDoc.hpp"

#include <score_test/Document.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/ImageNode.hpp>
#include <Threedim/ModelDisplay/ModelDisplayNode.hpp>
#include <Threedim/Primitive.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <QPainter>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

struct HalpProcesses
{
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  int next = 1;
  template <typename T>
  std::unique_ptr<score::gfx::Node> make(const score::DocumentContext& ctx)
  {
    auto model = std::make_unique<oscr::ProcessModel<T>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{next}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));
    return std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<T>{*raw, {}, Gfx::exec_controls{}, next++, ctx}};
  }
};

enum class Hue
{
  None,
  Red,
  Blue
};

Hue hue(const std::array<uint8_t, 4>& px)
{
  if(px[0] > 128 && px[2] < 64)
    return Hue::Red;
  if(px[2] > 128 && px[0] < 64)
    return Hue::Blue;
  return Hue::None;
}

QString red_over_blue_png()
{
  const QString dir = gfxproc::scratch_dir("images-orientation-fixl");
  QImage img{64, 64, QImage::Format_ARGB32};
  img.fill(qRgb(0, 0, 255));
  {
    QPainter p{&img};
    p.fillRect(0, 0, 64, 32, QColor{255, 0, 0});
  }
  const QString path = dir + "/red-over-blue.png";
  REQUIRE(img.save(path, "PNG"));
  return path;
}

void feed_images(score::gfx::Node& node, const QString& path)
{
  score::gfx::Message m;
  m.node_id = node.nodeId;
  m.input.resize(8);
  m.input[0] = ossia::value{0};
  m.input[1] = ossia::value{1.f};
  m.input[2] = ossia::value{ossia::vec2f{0.f, 0.f}};
  m.input[3] = ossia::value{1.f};
  m.input[4] = ossia::value{1.f};
  m.input[5] = ossia::value{std::vector<ossia::value>{path.toStdString()}};
  m.input[6] = ossia::value{0};
  m.input[7] = ossia::value{(int)score::gfx::ScaleMode::Stretch};
  node.process(std::move(m));
}

struct Shot
{
  bool skipped{};
  std::string error;
  ReadbackImage img;
};

enum class Route
{
  Direct,
  ThroughIsf,
  ModelDisplay
};

Shot shoot(Route route, score::gfx::GraphicsApi api, int proj = 0, bool isfSource = false)
{
  Shot s;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      s.error = "no document";
      return;
    }
    const QString png = red_over_blue_png();
    HalpProcesses procs;
    GfxPipeline p;
    int images = -1;
    if(isfSource)
      images = p.addIsf(corpus("syn-red-over-blue.fs"));
    else
    {
      images = p.addNode(std::make_unique<score::gfx::ImagesNode>(doc->context()));
      feed_images(*p.node(images), png);
    }
    const int sink = p.addSink({96, 96});

    switch(route)
    {
      case Route::Direct:
        p.wire(p.nodeImageOut(images), p.sinkInput(sink));
        break;
      case Route::ThroughIsf: {
        const int isf = p.addIsf(corpus("isf-passthrough-plain.fs"));
        if(isf < 0)
        {
          s.error = "isf build failed: " + p.error();
          return;
        }
        p.wire(p.nodeImageOut(images), p.imageIn(isf, 0));
        p.wire(p.imageOut(isf, 0), p.sinkInput(sink));
        break;
      }
      case Route::ModelDisplay: {
        const int cube = p.addNode(procs.make<Threedim::Cube>(doc->context()));
        auto md = std::make_unique<score::gfx::ModelDisplayNode>();
        md->texture_projection = proj;
        md->position = {0.f, 0.f, 2.5f};
        md->center = {0.f, 0.f, 0.f};
        md->fov = 60.f;
        auto* mdNode = md.get();
        const int display = p.addNode(std::move(md));
        if(cube < 0 || display < 0)
        {
          s.error = "node build failed: " + p.error();
          return;
        }
        p.wire(isfSource ? p.imageOut(images, 0) : p.nodeImageOut(images), mdNode->input[0]);
        p.wire(p.nodeSceneOut(cube, 0), mdNode->input[1]);
        p.wire(p.nodeImageOut(display, 0), p.sinkInput(sink));
        break;
      }
    }

    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.error = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    s.img = p.readback(sink);
    if(!s.img.valid())
      s.error = "empty readback";
  });
  return s;
}

void check_red_on_top(const Shot& s)
{
  if(s.skipped)
    SKIP("backend unavailable");
  INFO("error=" << s.error);
  REQUIRE(s.error.empty());

  int top = -1, bottom = -1, left = s.img.width, right = -1;
  for(int y = 0; y < s.img.height; y++)
    for(int x = 0; x < s.img.width; x++)
      if(hue(s.img.at(x, y)) != Hue::None)
      {
        if(top < 0)
          top = y;
        bottom = y;
        left = std::min(left, x);
        right = std::max(right, x);
      }
  INFO("coloured rows " << top << ".." << bottom << " cols " << left << ".." << right);
  REQUIRE(top >= 0);
  REQUIRE(bottom - top > 8);
  const int cx = (left + right) / 2;
  const int span = bottom - top;
  const auto upper = hue(s.img.at(cx, top + span / 4));
  const auto lower = hue(s.img.at(cx, bottom - span / 4));
  INFO("upper " << int(upper) << " lower " << int(lower));
  CHECK(upper == Hue::Red);
  CHECK(lower == Hue::Blue);
}
}

TEST_CASE("Images into a sink is upright", "[gfx][images][orientation]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  check_red_on_top(shoot(Route::Direct, api));
}

TEST_CASE("Images into an ISF image input is upright", "[gfx][images][orientation]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  check_red_on_top(shoot(Route::ThroughIsf, api));
}

TEST_CASE(
    "Images into ModelDisplay reads the same way up as ISF",
    "[gfx][images][orientation][modeldisplay]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  check_red_on_top(shoot(Route::ModelDisplay, api));
}

TEST_CASE(
    "Images and ISF give ModelDisplay the same picture under every projection",
    "[gfx][images][orientation][modeldisplay]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const int proj = GENERATE(0, 1, 2, 3, 4, 5);
  CAPTURE(backend_name(api), proj);
  const auto img = shoot(Route::ModelDisplay, api, proj, false);
  const auto isf = shoot(Route::ModelDisplay, api, proj, true);
  if(img.skipped || isf.skipped)
    SKIP("backend unavailable");
  INFO("errors=" << img.error << " / " << isf.error);
  REQUIRE(img.error.empty());
  REQUIRE(isf.error.empty());
  REQUIRE(img.img.width == isf.img.width);
  REQUIRE(img.img.height == isf.img.height);
  int compared = 0, swapped = 0;
  for(int y = 0; y < img.img.height; y++)
    for(int x = 0; x < img.img.width; x++)
    {
      const auto a = hue(img.img.at(x, y));
      const auto b = hue(isf.img.at(x, y));
      if(a == Hue::None || b == Hue::None)
        continue;
      compared++;
      if(a != b)
        swapped++;
    }
  INFO("compared " << compared << " swapped " << swapped);
  CHECK(swapped <= compared / 50);
}

TEST_CASE(
    "ModelDisplay gives the same picture on every backend under every projection",
    "[gfx][images][orientation][modeldisplay]")
{
  const int proj = GENERATE(0, 1, 2, 3, 4, 5);
  const bool isf = GENERATE(false, true);
  CAPTURE(proj, isf);
  const auto backends = platform_backends();
  if(backends.size() < 2)
    SKIP("needs two backends");
  const auto ref = shoot(Route::ModelDisplay, backends[0], proj, isf);
  if(ref.skipped)
    SKIP("backend unavailable");
  INFO("reference " << backend_name(backends[0]) << " error=" << ref.error);
  REQUIRE(ref.error.empty());
  for(std::size_t i = 1; i < backends.size(); i++)
  {
    CAPTURE(backend_name(backends[i]));
    const auto other = shoot(Route::ModelDisplay, backends[i], proj, isf);
    if(other.skipped)
      continue;
    INFO("error=" << other.error);
    REQUIRE(other.error.empty());
    REQUIRE(other.img.width == ref.img.width);
    REQUIRE(other.img.height == ref.img.height);
    int compared = 0, swapped = 0;
    for(int y = 0; y < ref.img.height; y++)
      for(int x = 0; x < ref.img.width; x++)
      {
        const auto a = hue(ref.img.at(x, y));
        const auto b = hue(other.img.at(x, y));
        if(a == Hue::None || b == Hue::None)
          continue;
        compared++;
        if(a != b)
          swapped++;
      }
    INFO("compared " << compared << " swapped " << swapped);
    CHECK(compared > 0);
    CHECK(swapped <= compared / 50);
  }
}

TEST_CASE(
    "ModelDisplay's spherical projection puts the top of the image on top",
    "[gfx][images][orientation][modeldisplay]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool isf = GENERATE(false, true);
  CAPTURE(backend_name(api), isf);
  check_red_on_top(shoot(Route::ModelDisplay, api, 2, isf));
}
