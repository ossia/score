// Every CSF compute shader in the library produces geometry a rasterizer can draw.
//
// The plain CSF sweep (ShaderSweepCSF.cpp) wires each .cs to an Image sink, but
// not one of the 40 testers declares an OUTPUTS block: they are compute shaders
// whose only RESOURCE is `"TYPE": "geometry"`, so they write vertex buffers and
// never a texture. textureForOutput() correctly returns null and the node warns
// "No output texture available for graphics pass" — the sweep was asking every
// shader in the corpus a question none of them answers, and scoring the silence
// as 31 failures.
//
// So drive them the way the corpus says to. raw-raster-basic.fs names its own
// producers in its DESCRIPTION ("Connect any geometry producer with
// position+color semantics (e.g. 1d-no-stride.cs, csf-vertex-count-expr.cs,
// csf-multi-geometry.cs) to this node, then connect this node to a Window"),
// which is exactly the chain render_raster() builds:
//
//     CSF tester --Geometry--> raw-raster consumer --Image--> sink
//
// This is ShaderSweepRaster.cpp inverted: there the raster shader varies over a
// fixed producer, here the producer varies over a fixed raster consumer. Between
// them every .cs and every RAW_RASTER_PIPELINE .fs in the corpus gets rasterized
// and compared as pixels rather than as an exit code.

#include "ShaderSweep.hpp"

#include <score_test/Gfx.hpp>

using namespace score::test::gfx;

namespace
{
//! A frame with no pixels at all is NOT uniform — it is a failure. Reporting it
//! as a pass is how a shader that renders nothing scores green.
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

//! The fixed consumer. Declares `position + color` VERTEX_INPUTS, which is the
//! semantic pair the geometry testers emit.
const char* kRasterConsumer = "raw-raster-basic";

//! The .cs corpus splits in two by what a shader writes: 22 declare a
//! `"TYPE": "geometry"` RESOURCE and drive a rasterizer, 17 declare only images
//! and are read back directly. Wiring an image producer to a raster node yields
//! "geometry producer ... has no Geometry output port", which is a routing
//! mistake on our side, not a defect in the shader — so each sweep takes the
//! half it can actually answer for.
inline bool declaresGeometryResource(const QByteArray& data)
{
  static const QRegularExpression re{R"_("TYPE"\s*:\s*"geometry")_"};
  return re.match(QString::fromUtf8(data.left(8192))).hasMatch();
}

inline void sweepCSFGeometry(
    const score::GUIApplicationContext& ctx, const QString& baseline,
    const QString& consumerName)
{
  const QString root = libraryRoot(ctx);
  if(root.isEmpty() || !QFileInfo::exists(root))
    SKIP("no shader library available (set SCORE_SHADER_LIBRARY_DIR)");

  const auto* gfx_settings = ctx.findSettings<Gfx::Settings::Model>();
  if(!gfx_settings)
    FAIL("score_plugin_gfx registered no settings model: run from the build root.");
  const auto api = gfx_settings->graphicsApiEnum();

  // Locate the consumer pair; without it there is nothing to rasterize into.
  QString consumerFs;
  {
    QDirIterator it{root,
                    {consumerName + QStringLiteral(".fs")},
                    QDir::Files,
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks};
    if(it.hasNext())
      consumerFs = it.next();
  }
  if(consumerFs.isEmpty())
    SKIP(
        "raster consumer " << consumerName.toStdString()
                           << ".fs not found in the library");

  QString consumerVs = consumerFs;
  consumerVs.replace(QStringLiteral(".fs"), QStringLiteral(".vs"));
  if(!QFileInfo::exists(consumerVs))
    SKIP(
        "raster consumer " << consumerName.toStdString()
                           << ".vs not found beside its fragment shader");

  QStringList shaders;
  QDirIterator it{root, {"*.cs", "*.comp", "*.csf"}, QDir::Files,
                  QDirIterator::Subdirectories | QDirIterator::FollowSymlinks};
  while(it.hasNext())
    shaders.push_back(it.next());
  shaders.sort();

  std::map<QString, std::map<std::string, std::string>> failures;

  for(const QString& cs : shaders)
  {
    QFile f{cs};
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;
    const QByteArray src = f.readAll();
    if(shaderMode(src) != QStringLiteral("COMPUTE_SHADER"))
      continue;
    if(!declaresGeometryResource(src))
      continue;

    const QString rel = QDir{root}.relativeFilePath(cs);
    qInfo().noquote() << "[sweep]" << rel;

    const auto r = render_raster(api, {cs}, consumerVs, consumerFs);
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
}

TEST_CASE(
    "Every CSF shader in the library produces drawable geometry",
    "[integration][gfx][shaders]")
{
  requestGlesContext();
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    sweepCSFGeometry(
        ctx, QStringLiteral(SCORE_SHADER_SWEEP_BASELINE_CSFGEOMETRY),
        QString::fromUtf8(kRasterConsumer));
  });
}
