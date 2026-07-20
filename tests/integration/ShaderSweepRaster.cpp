// Every RAW_RASTER_PIPELINE shader in the library rasterizes.
//
// A raster shader draws geometry, so — unlike ISF — it cannot be validated by
// handing it a full-screen quad. This sweep builds the same chain the JS testers
// build (tests-scene/common.js buildWithCube / buildWithVertexColorProducer):
//
//     CSF geometry producer --Geometry--> raw-raster node --Image--> sink
//
// through the render_raster() fixture, which is what tests/gfx/GfxRaster.cpp
// already uses for the hand-picked cases. Before this existed the raster shaders
// were swept as ISF purely because they are also .fs files: all 41 failed to bake
// with "'position' : undeclared identifier", and RenderedRawRasterPipelineNode
// (2024 lines) had never been executed by a test at all.
//
// Every raster tester in the corpus ships its own .vs sibling (41/41) and none is
// procedural, so a geometry producer is always required.

#include "ShaderSweep.hpp"

#include <score_test/Gfx.hpp>

using namespace score::test::gfx;

namespace
{
//! Same "did it draw anything" test as isUniform(), for the fixture's readback
//! type. A frame with no pixels at all is NOT uniform — it is a failure, and
//! reporting it as a pass is how a shader that renders nothing scores green.
inline bool isUniformImage(const ReadbackImage& im)
{
  if(!im.valid() || im.bytes.isEmpty())
    return false;
  const auto* p = reinterpret_cast<const quint32*>(im.bytes.constData());
  const auto n = im.bytes.size() / 4;
  for(qsizetype i = 1; i < n; i++)
    if(p[i] != p[0])
      return false;
  return true;
}

//! Raster shaders draw geometry, so they cannot be validated with a full-screen
//! quad the way ISF is. This builds the same chain the JS testers do —
//! CSF geometry producer -> raw-raster node -> sink — via the render_raster()
//! fixture, once per shader, and diffs the outcome against the baseline.
inline void sweepRaster(
    const score::GUIApplicationContext& ctx, const QString& baseline,
    const QString& producerName)
{
  const QString root = libraryRoot(ctx);
  if(root.isEmpty() || !QFileInfo::exists(root))
    SKIP("no shader library available (set SCORE_SHADER_LIBRARY_DIR)");

  const auto* gfx_settings = ctx.findSettings<Gfx::Settings::Model>();
  if(!gfx_settings)
    FAIL("score_plugin_gfx registered no settings model: run from the build root.");
  const auto api = gfx_settings->graphicsApiEnum();

  // The producer lives in the library too; without it there is nothing to draw.
  QString producer;
  {
    QDirIterator it{root, {producerName}, QDir::Files,
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks};
    if(it.hasNext())
      producer = it.next();
  }
  if(producer.isEmpty())
    SKIP(
        "geometry producer " << producerName.toStdString()
                            << " not found in the library");

  QStringList shaders;
  QDirIterator it{root, {"*.fs", "*.frag"}, QDir::Files,
                  QDirIterator::Subdirectories | QDirIterator::FollowSymlinks};
  while(it.hasNext())
    shaders.push_back(it.next());
  shaders.sort();

  std::map<QString, std::map<std::string, std::string>> failures;

  for(const QString& fs : shaders)
  {
    QFile f{fs};
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;
    if(shaderMode(f.readAll()) != QStringLiteral("RAW_RASTER_PIPELINE"))
      continue;

    const QString rel = QDir{root}.relativeFilePath(fs);
    qInfo().noquote() << "[sweep]" << rel;

    // Every raster tester ships its own vertex stage; the ISF default does not
    // declare the attributes a rasterizer reads.
    QString vs = fs;
    vs.replace(QStringLiteral(".frag"), QStringLiteral(".vert"));
    vs.replace(QStringLiteral(".fs"), QStringLiteral(".vs"));
    if(vs == fs || !QFileInfo::exists(vs))
    {
      failures[rel]["novertex"] = "no vertex shader beside the fragment shader";
      report(rel, failures[rel]);
      continue;
    }

    const auto r = render_raster(api, {producer}, vs, fs);
    if(r.skipped)
      continue;
    if(!r.error.empty())
      failures[rel]["render"] = r.error;
    else if(r.outputs.empty())
      failures[rel]["render"] = "no output attachment read back";
    else if(isUniformImage(r.outputs.front()))
      failures[rel]["blank"] = "every pixel identical";

    if(auto it2 = failures.find(rel); it2 != failures.end())
      report(rel, it2->second);
  }

  diffAgainstBaseline(failures, baseline);
}

//! The stock geometry producer the JS testers use for raster shaders. Emits a
//! `color` vertex attribute as well as position, which the rasterizers that
//! declare `position + color` VERTEX_INPUTS need (Primitive Cube does not).
const char* kGeometryProducer = "csf-vertex-count-expr.cs";
}

TEST_CASE(
    "Every raster-pipeline shader in the library rasterizes",
    "[integration][gfx][shaders]")
{
  requestGlesContext();
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    sweepRaster(
        ctx, QStringLiteral(SCORE_SHADER_SWEEP_BASELINE_RASTER),
        QString::fromUtf8(kGeometryProducer));
  });
}
