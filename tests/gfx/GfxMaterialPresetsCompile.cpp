// The material presets that sample the shared texture pool compile and build
// a pipeline on every available backend.
//
// They live in the user library (presets/rasterizers), not in this
// repository, and read the pool through hand-written helpers -- the bucket
// ladder, scene_material_uv_xforms, scene_material_wrap -- that no engine
// change can type-check for them. A helper signature they disagree with fails
// only at shader compile, as a node that draws nothing.
#include "GfxUserLibrary.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <score_test/Gfx.hpp>

#include <string_view>

using namespace score::test::gfx;

TEST_CASE(
    "the pool-sampling material presets compile and build a pipeline",
    "[gfx][presets][material]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  // The shading tiers that sample the pool (csf-examples d8e4b99 / 3b30283
  // replaced classic_pbr_full and classic_pbr_textured, archived).
  const auto preset = GENERATE(
      std::pair{"classic_pbr.vert", "classic_pbr.frag"},
      std::pair{"classic_pbr_cube.vert", "classic_pbr_cube.frag"},
      std::pair{"classic_pbr_openpbr.vert", "classic_pbr_openpbr.frag"},
      std::pair{"classic_pbr_skinned.vert", "classic_pbr_skinned.frag"},
      std::pair{"pbr_fast.vert", "pbr_fast.frag"},
      std::pair{"blinn_phong.vert", "blinn_phong.frag"},
      std::pair{"g6_cluster_probe.vert", "g6_cluster_probe.frag"});
  CAPTURE(backend_name(api));
  CAPTURE(preset.second);

  const QString rel = QStringLiteral("presets/rasterizers/");
  const QString vs = library::find(rel + QLatin1String(preset.first));
  const QString fs = library::find(rel + QLatin1String(preset.second));
  // Without a library there is nothing to compile; with one, a preset it does
  // not have is a stale list, not a skip (this list named two archived
  // presets and skipped in silence).
  if(library::root().isEmpty())
    SKIP(library::skip_reason(rel + QLatin1String(preset.second)));
  INFO(library::skip_reason(rel + QLatin1String(preset.second)));
  REQUIRE(!vs.isEmpty());
  REQUIRE(!fs.isEmpty());
  // Not a SKIP: one skipped generator value reports the whole case skipped,
  // which hid every other preset's result.
  if(api == score::gfx::OpenGL
     && std::string_view{preset.second} == "classic_pbr_openpbr.frag")
  {
    SUCCEED("classic_pbr_openpbr binds more fragment storage buffers than OpenGL offers");
    return;
  }

  bool skipped = false;
  std::string err;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    library::RootGuard root{ctx};

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
