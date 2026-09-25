// A geometry filter's own #define PI does not break ModelDisplay.
//
// ModelDisplay splices every geometry filter's source into its vertex shader
// ahead of the projection code, so the projection code must not declare an
// identifier named PI. tests/threedim/define-pi-filter.glsl
// (outside the corpus, which pins its geometry filter count) scales every
// vertex by sin(PI * k); through a fulldome ModelDisplay, k = 0.5 must draw the
// textured cube with no bake failure, and k = 1 must draw nothing, which proves
// the filter code is really in the stage.
#include "GfxHalpNodes.hpp"
#include "GfxLogCapture.hpp"
#include "IsfTestCommon.hpp"

#include <score_test/Document.hpp>

#include <Gfx/Graph/GeometryFilterNode.hpp>

#include <Threedim/ModelDisplay/ModelDisplayNode.hpp>
#include <Threedim/Primitive.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFile>

#include <isf.hpp>

#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

namespace
{
std::unique_ptr<score::gfx::GeometryFilterNode>
makeFilter(const QString& path, int index, std::string& err)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
  {
    err = "cannot open " + path.toStdString();
    return {};
  }
  try
  {
    ::isf::parser p{
        QString::fromUtf8(f.readAll()).toStdString(),
        ::isf::parser::ShaderType::GeometryFilter};
    return std::make_unique<score::gfx::GeometryFilterNode>(
        int64_t(index), p.data(), QString::fromStdString(p.geometry_filter()));
  }
  catch(const std::exception& e)
  {
    err = std::string("geometry filter parse failed: ") + e.what();
  }
  return {};
}

struct Shot
{
  bool skipped{};
  std::string error;
  int coloured = 0;
  int bakeFailures = 0;
};

Shot shoot(score::gfx::GraphicsApi api, float k)
{
  Shot s;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    LogCapture log;
    auto* doc = new_document(app);
    if(!doc)
    {
      s.error = "no document";
      return;
    }
    HalpProcesses procs;
    GfxPipeline p;
    const int image = p.addIsf(corpus("syn-red-over-blue.fs"));
    const int cube = p.addNode(procs.make<Threedim::Cube>(doc->context()));
    auto filter = makeFilter(
        QStringLiteral(THREEDIM_TEST_FIXTURE_DIR "/define-pi-filter.glsl"), 7, s.error);
    if(!filter)
      return;
    auto* filterNode = filter.get();
    const int filt = p.addNode(std::move(filter));
    auto md = std::make_unique<score::gfx::ModelDisplayNode>();
    md->texture_projection = 0;
    md->camera_mode = 1;
    md->position = {0.f, 0.f, 2.5f};
    md->center = {0.f, 0.f, 0.f};
    md->fov = 90.f;
    auto* mdNode = md.get();
    const int display = p.addNode(std::move(md));
    if(image < 0 || cube < 0 || filt < 0 || display < 0)
    {
      s.error = "node build failed: " + p.error();
      return;
    }
    auto* fin = p.nodeGeometryIn(filt, 0);
    auto* fout = p.nodeGeometryOut(filt, 0);
    if(!fin || !fout || filterNode->input.size() < 2)
    {
      s.error = "the geometry filter has no geometry ports or no k control";
      return;
    }
    p.wire(p.imageOut(image, 0), mdNode->input[0]);
    p.wire(p.nodeSceneOut(cube, 0), fin);
    p.wire(fout, mdNode->input[1]);
    static_cast<score::gfx::ProcessNode&>(*filterNode).process(int32_t(1), ossia::value{k});
    const int sink = p.addSink({96, 96});
    p.wire(p.nodeImageOut(display, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.error = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      s.error = "empty readback";
      return;
    }
    for(int y = 0; y < img.height; y++)
      for(int x = 0; x < img.width; x++)
      {
        const auto px = img.at(x, y);
        if((px[0] > 128 && px[2] < 64) || (px[2] > 128 && px[0] < 64))
          s.coloured++;
      }
    s.bakeFailures = log.count({u"bake failed"});
  });
  return s;
}
}

TEST_CASE(
    "a geometry filter's #define PI does not break a fulldome ModelDisplay",
    "[gfx][threedim][modeldisplay][geometryfilter]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Shot kept = shoot(api, 0.5f);
  if(kept.skipped)
    SKIP("backend unavailable");
  INFO("error=" << kept.error);
  REQUIRE(kept.error.empty());
  CHECK(kept.bakeFailures == 0);
  CHECK(kept.coloured > 100);

  const Shot collapsed = shoot(api, 1.0f);
  INFO("error=" << collapsed.error);
  REQUIRE(collapsed.error.empty());
  CHECK(collapsed.bakeFailures == 0);
  CHECK(collapsed.coloured == 0);
}
