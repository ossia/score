// The material presets that sample the shared texture pool compile and build
// a pipeline on every available backend.
//
// They live in the user library (csf-examples/presets/rasterizers), not in
// this repository, and read the pool through hand-written helpers -- the
// bucket ladder, scene_material_uv_xforms, scene_material_wrap -- that no
// engine change can type-check for them. A helper signature they disagree
// with fails only at shader compile, as a node that draws nothing.
//
// The directory comes from SCORE_CSF_PRESETS, else the default library
// location; without it the test skips. Some presets include "openpbr.h" from
// the library's packages/ tree, so the library root the presets sit in is made
// the application's library for the duration of each case.
//
// Registration:
//   score_add_gfx_test(material_presets_compile GfxMaterialPresetsCompile.cpp)
#include <score_test/Gfx.hpp>

#include <Library/LibrarySettings.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QDir>
#include <QFileInfo>

using namespace score::test::gfx;

namespace
{
QString presetDir()
{
  const QString env = qEnvironmentVariable("SCORE_CSF_PRESETS");
  if(!env.isEmpty())
    return env;
  return QDir::homePath()
         + QStringLiteral(
             "/Documents/ossia/score/packages/csf-examples/presets/rasterizers");
}

QString libraryRoot()
{
  QDir root(presetDir());
  return root.cd(QStringLiteral("../../../..")) ? root.absolutePath() : QString{};
}
}

TEST_CASE(
    "the pool-sampling material presets compile and build a pipeline",
    "[gfx][presets][material]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto preset = GENERATE(
      std::pair{"classic_pbr_full.vert", "classic_pbr_full.frag"},
      std::pair{"classic_pbr_full.vert", "classic_pbr_full_b4.frag"},
      std::pair{"classic_pbr_openpbr.vert", "classic_pbr_openpbr.frag"},
      std::pair{"classic_pbr_textured.vert", "classic_pbr_textured.frag"},
      std::pair{"classic_pbr_skinned.vert", "classic_pbr_skinned.frag"},
      std::pair{"g6_cluster_probe.vert", "g6_cluster_probe.frag"});
  CAPTURE(backend_name(api));
  CAPTURE(preset.second);

  const QDir dir(presetDir());
  const QString vs = dir.filePath(QString::fromLatin1(preset.first));
  const QString fs = dir.filePath(QString::fromLatin1(preset.second));
  if(!QFileInfo::exists(vs) || !QFileInfo::exists(fs))
    SKIP("preset library not found at " + presetDir().toStdString());

  bool skipped = false;
  std::string err;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto& lib = ctx.settings<Library::Settings::Model>();
    const QString previousRoot = lib.getRootPath();
    lib.setRootPath(libraryRoot());
    struct Restore
    {
      Library::Settings::Model& lib;
      QString root;
      ~Restore() { lib.setRootPath(root); }
    } restore{lib, previousRoot};

    GfxPipeline p;
    const int raster = p.addRaster(vs, fs);
    if(raster < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(2);
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO(err);
  CHECK(err.empty());
}
