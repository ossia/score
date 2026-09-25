// Cubemap Loader: every layout lands each direction where cubemap_view reads it.
//
// The source images are coloured by direction, rgb = dir * 0.5 + 0.5:
//  * Equirectangular: the panorama convention of cubemap_view.fs, centre = -Z,
//    +X a quarter turn to the right, top = +Y.
//  * Horizontal / vertical cross: each cell holds its face in the cube-map
//    (s, t) orientation, except the -Z cell of the vertical cross, which the
//    standard layout stores rotated by 180 degrees.
// The loaded cube is unwrapped by e3-cubemap-equirect-view.fs (the same
// mapping as cubemap_view.fs) and every pixel must read back its own
// direction. Before the fix the equirectangular cube was turned a quarter turn
// around +Y and the vertical-cross -Z face was upside down.
//
// Registration: see the test_gfx_cubemap_loader_orientation_e3 target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Threedim/CubemapLoader.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <QImage>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
using Dir = std::array<double, 3>;
constexpr double kPi = 3.14159265358979323846;
constexpr int kFace = 64;
constexpr int kViewW = 128;
constexpr int kViewH = 64;

Dir normalized(Dir d)
{
  const double n = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  return {d[0] / n, d[1] / n, d[2] / n};
}

Dir panoramaDir(double u, double vUp)
{
  const double theta = (u * 2. - 1.) * kPi;
  const double phi = (vUp - 0.5) * kPi;
  return {
      std::cos(phi) * std::sin(theta), std::sin(phi),
      -std::cos(phi) * std::cos(theta)};
}

Dir faceDir(int face, double s, double t)
{
  const double sc = s * 2. - 1.;
  const double tc = t * 2. - 1.;
  switch(face)
  {
    case 0:
      return normalized({1., -tc, -sc});
    case 1:
      return normalized({-1., -tc, sc});
    case 2:
      return normalized({sc, 1., tc});
    case 3:
      return normalized({sc, -1., -tc});
    case 4:
      return normalized({sc, -tc, 1.});
    default:
      return normalized({-sc, -tc, -1.});
  }
}

QRgb encode(const Dir& d)
{
  auto c = [](double v) { return int(std::lround((v * 0.5 + 0.5) * 255.)); };
  return qRgba(c(d[0]), c(d[1]), c(d[2]), 255);
}

QImage makeEquirect()
{
  QImage img(256, 128, QImage::Format_RGBA8888);
  for(int y = 0; y < img.height(); ++y)
    for(int x = 0; x < img.width(); ++x)
      img.setPixel(
          x, y,
          encode(panoramaDir(
              (x + 0.5) / img.width(), 1. - (y + 0.5) / img.height())));
  return img;
}

QImage makeCross(Threedim::CubemapLayout layout)
{
  const bool vertical = layout == Threedim::CubemapLayout::VerticalCross;
  QImage img(
      kFace * (vertical ? 3 : 4), kFace * (vertical ? 4 : 3),
      QImage::Format_RGBA8888);
  img.fill(Qt::black);
  static constexpr int hx[6]{2, 0, 1, 1, 1, 3};
  static constexpr int hy[6]{1, 1, 0, 2, 1, 1};
  static constexpr int vx[6]{2, 0, 1, 1, 1, 1};
  static constexpr int vy[6]{1, 1, 0, 2, 1, 3};
  for(int face = 0; face < 6; ++face)
  {
    const int cx = (vertical ? vx : hx)[face] * kFace;
    const int cy = (vertical ? vy : hy)[face] * kFace;
    const bool rotated = vertical && face == 5;
    for(int j = 0; j < kFace; ++j)
      for(int i = 0; i < kFace; ++i)
      {
        const int si = rotated ? kFace - 1 - i : i;
        const int tj = rotated ? kFace - 1 - j : j;
        img.setPixel(
            cx + i, cy + j,
            encode(faceDir(face, (si + 0.5) / kFace, (tj + 0.5) / kFace)));
      }
  }
  return img;
}

struct TestCubemapLoader : Threedim::CubemapLoader
{
  static inline QImage s_image;
  static inline Threedim::CubemapLayout s_layout{};

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
  {
    m_loadedImage = s_image;
    Threedim::CubemapLoader::init(r, res);
  }

  void runInitialPasses(
      score::gfx::RenderList& r, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res,
      score::gfx::Edge& e)
  {
    inputs.layout.value = s_layout;
    inputs.resolution.value = kFace;
    Threedim::CubemapLoader::runInitialPasses(r, cb, res, e);
  }
};

struct Outcome
{
  bool skipped = false;
  std::string error;
  ReadbackImage view;
};

Outcome run(score::gfx::GraphicsApi api, Threedim::CubemapLayout layout)
{
  Outcome out;
  TestCubemapLoader::s_layout = layout;
  TestCubemapLoader::s_image = layout == Threedim::CubemapLayout::Equirectangular
                                   ? makeEquirect()
                                   : makeCross(layout);
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    const auto& ctx = doc->context();
    auto model = std::make_unique<oscr::ProcessModel<TestCubemapLoader>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));

    GfxPipeline p;
    const int loader = p.addNode(std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<TestCubemapLoader>{
            *raw, {}, Gfx::exec_controls{}, 1, ctx}});
    const int view = p.addIsf(
        QStringLiteral(GFX_TEST_CORPUS_DIR "/e3-cubemap-equirect-view.fs"));
    if(loader < 0 || view < 0)
    {
      out.error = "node build failed: " + p.error();
      return;
    }
    p.wire(p.nodeImageOut(loader, 0), p.imageIn(view, 0));
    const int sink = p.addSink({kViewW, kViewH});
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    out.view = p.readback(sink);
    if(!out.view.valid())
      out.error = "empty readback";
  });
  return out;
}

int dominantFace(const Dir& d)
{
  const double ax = std::abs(d[0]), ay = std::abs(d[1]), az = std::abs(d[2]);
  if(ax >= ay && ax >= az)
    return d[0] > 0 ? 0 : 1;
  if(ay >= az)
    return d[1] > 0 ? 2 : 3;
  return d[2] > 0 ? 4 : 5;
}

const char* const kFaceName[6]{"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
}

TEST_CASE(
    "Cubemap Loader layouts agree with the cubemap_view convention",
    "[gfx][threedim][cubemap]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto layout = GENERATE(
      Threedim::CubemapLayout::Equirectangular,
      Threedim::CubemapLayout::HorizontalCross,
      Threedim::CubemapLayout::VerticalCross);
  CAPTURE(backend_name(api));
  CAPTURE(int(layout));

  const Outcome out = run(api, layout);
  if(out.skipped)
    SKIP("no usable RHI backend");
  REQUIRE(out.error.empty());
  REQUIRE(out.view.width == kViewW);
  REQUIRE(out.view.height == kViewH);

  double err[6]{};
  int count[6]{};
  for(int y = 0; y < kViewH; ++y)
    for(int x = 0; x < kViewW; ++x)
    {
      const Dir d
          = panoramaDir((x + 0.5) / kViewW, 1. - (y + 0.5) / kViewH);
      const int face = dominantFace(d);
      const double axis = std::max(
          {std::abs(d[0]), std::abs(d[1]), std::abs(d[2])});
      if(axis < 0.75)
        continue;
      const auto px = out.view.at(x, y);
      double e = 0.;
      for(int c = 0; c < 3; ++c)
        e += std::abs(px[c] / 255. * 2. - 1. - d[c]);
      err[face] += e / 3.;
      count[face]++;
    }
  for(int face = 0; face < 6; ++face)
  {
    CAPTURE(kFaceName[face]);
    REQUIRE(count[face] > 0);
    CHECK(err[face] / count[face] < 0.05);
  }
}
