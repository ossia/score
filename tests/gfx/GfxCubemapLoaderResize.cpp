// Cubemap Loader: changing the Resolution replaces the cube texture, and every
// consumer must sample the new one. The old cube is released on the frame of the
// change, so a consumer still binding it samples a destroyed texture.
//
// Three consumers of the same loader:
//  * a procedural raw raster whose "skybox" INPUT is resolved by name from the
//    Scene Preprocessor's geometry (the loader's Scene outlet, no cable);
//  * the same raster with the loader's Cubemap outlet cabled to that INPUT;
//  * an ISF unwrap with the Cubemap outlet cabled to its cube INPUT.
// Each phase changes the resolution and the solid colour of the source image
// together, so the readback tells the new cube from the old one. Then the
// loader is disconnected, which releases its renderer and its cube: the
// consumer must fall back to its black empty cube.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Threedim/CubemapLoader.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <QImage>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
constexpr int kView = 32;

constexpr std::array<int, 3> kResolutions{32, 128, 16};
const std::array<QColor, 3> kColours{
    QColor{255, 0, 0}, QColor{0, 255, 0}, QColor{0, 0, 255}};

// Stands in for the file port and the Resolution spinbox: a new value reaches
// the node the way their update() callbacks deliver it, by setting
// m_imageChanged before the next runInitialPasses.
struct ResizedCubemapLoader : Threedim::CubemapLoader
{
  static inline int s_phase{};
  int m_seenPhase{-1};

  void runInitialPasses(
      score::gfx::RenderList& r, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res,
      score::gfx::Edge& e)
  {
    if(m_seenPhase != s_phase)
    {
      m_seenPhase = s_phase;
      QImage img(64, 32, QImage::Format_RGBA8888);
      img.fill(kColours[s_phase]);
      m_loadedImage = std::move(img);
      inputs.layout.value = Threedim::CubemapLayout::Equirectangular;
      inputs.resolution.value = kResolutions[s_phase];
      m_imageChanged = true;
    }
    Threedim::CubemapLoader::runInitialPasses(r, cb, res, e);
  }
};

enum class Consumer
{
  RasterThroughScene,
  RasterThroughCable,
  IsfThroughCable,
};

const char* consumerName(Consumer c)
{
  switch(c)
  {
    case Consumer::RasterThroughScene:
      return "raster, skybox from the Scene Preprocessor";
    case Consumer::RasterThroughCable:
      return "raster, Cubemap cable";
    default:
      return "ISF, Cubemap cable";
  }
}

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::vector<ReadbackImage> views;
};

Outcome run(score::gfx::GraphicsApi api, Consumer consumer)
{
  Outcome out;
  ResizedCubemapLoader::s_phase = 0;
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    const auto& ctx = doc->context();
    auto model = std::make_unique<oscr::ProcessModel<ResizedCubemapLoader>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));

    GfxPipeline p;
    const int loader = p.addNode(std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<ResizedCubemapLoader>{
            *raw, {}, Gfx::exec_controls{}, 1, ctx}});
    const int view
        = consumer == Consumer::IsfThroughCable
              ? p.addIsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/cubemap-equirect-view.fs"))
              : p.addRaster(
                    QStringLiteral(GFX_TEST_CORPUS_DIR "/cubemap-skybox-probe.vs"),
                    QStringLiteral(GFX_TEST_CORPUS_DIR "/cubemap-skybox-probe.fs"));
    if(loader < 0 || view < 0)
    {
      out.error = "node build failed: " + p.error();
      return;
    }

    // The edge leaving the loader, cut in the last phase.
    score::gfx::Port* loaderOut{};
    score::gfx::Port* loaderSink{};
    if(consumer == Consumer::RasterThroughScene)
    {
      const int flat
          = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
      loaderOut = p.nodeSceneOut(loader, 0);
      loaderSink = p.nodeSceneIn(flat, 0);
      auto* flatOut = p.nodeGeometryOut(flat, 0);
      if(flat < 0 || !loaderOut || !loaderSink || !flatOut)
      {
        out.error = "scene ports missing: " + p.error();
        return;
      }
      p.wire(flatOut, p.geometryIn(view, 0));
    }
    else
    {
      loaderOut = p.nodeImageOut(loader, 0);
      loaderSink = p.imageIn(view, 0);
    }
    p.wire(loaderOut, loaderSink);
    const int sink = p.addSink({kView, kView});
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }

    for(int phase = 0; phase < int(kResolutions.size()); ++phase)
    {
      ResizedCubemapLoader::s_phase = phase;
      p.render(4);
      out.views.push_back(p.readback(sink));
    }

    p.removeEdgeIncremental(loaderOut, loaderSink);
    p.render(4);
    out.views.push_back(p.readback(sink));
  });
  return out;
}
}

TEST_CASE(
    "Cubemap Loader resolution change rebinds the new cube in every consumer",
    "[gfx][threedim][cubemap]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto consumer = GENERATE(
      Consumer::RasterThroughScene, Consumer::RasterThroughCable,
      Consumer::IsfThroughCable);
  CAPTURE(backend_name(api));
  CAPTURE(consumerName(consumer));

  const Outcome out = run(api, consumer);
  if(out.skipped)
    SKIP(out.skip_reason);
  REQUIRE(out.error.empty());
  REQUIRE(out.views.size() == kResolutions.size() + 1);

  for(std::size_t phase = 0; phase < out.views.size(); ++phase)
  {
    const bool disconnected = phase == kResolutions.size();
    CAPTURE(phase, disconnected);
    const auto& img = out.views[phase];
    REQUIRE(img.valid());
    const auto px = img.center();
    CAPTURE(int(px[0]), int(px[1]), int(px[2]));
    const QColor want = disconnected ? QColor{0, 0, 0} : kColours[phase];
    CHECK(std::abs(px[0] - want.red()) <= 8);
    CHECK(std::abs(px[1] - want.green()) <= 8);
    CHECK(std::abs(px[2] - want.blue()) <= 8);
  }
}
