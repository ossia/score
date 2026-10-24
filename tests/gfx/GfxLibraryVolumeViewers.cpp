// =============================================================================
// The library's 3D viewers show a volume the library's generators make.
//
// volumetric/18-viewers/*.fs sample a `DIMENSIONS: 3` image input. The ISF
// sweep wires a 2D test card into every image input, which a sampler3D input
// cannot use: there they render blank on every backend, and say nothing about
// 3D textures. Here each viewer is fed what it is made for, a 3D_IMAGE CSF
// generator (01-noise/01-01_perlin3d.cs), and must show a non-flat image.
//
// Skips without SCORE_TEST_LIBRARY_ROOT (see GfxUserLibrary.hpp).
//
// Registration:
//   score_add_gfx_test(library_volume_viewers GfxLibraryVolumeViewers.cpp)
#include "GfxUserLibrary.hpp"

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QDir>

namespace
{
using score::test::gfx::IsfResult;
using score::test::gfx::near;
using score::test::gfx::ReadbackImage;

constexpr auto viewersDir = "Presets/GLSL_shaders/volumetric/18-viewers/18-01_slice_viewer.fs";
constexpr auto generator = "Presets/GLSL_shaders/volumetric/01-noise/01-01_perlin3d.cs";

int distinct_samples(const ReadbackImage& img)
{
  std::vector<std::array<uint8_t, 4>> seen;
  for(int y = 2; y < img.height - 2; y += 4)
    for(int x = 2; x < img.width - 2; x += 4)
    {
      const auto px = img.at(x, y);
      bool known = false;
      for(const auto& s : seen)
        if(near(px, s, 6))
        {
          known = true;
          break;
        }
      if(!known)
        seen.push_back(px);
      if(seen.size() > 8)
        return int(seen.size());
    }
  return int(seen.size());
}
}

TEST_CASE("every library 3D viewer shows a generated volume", "[gfx][library][3d]")
{
  namespace lib = score::test::gfx::library;
  const QString gen = lib::find(generator);
  const QString dir = lib::dir_of(viewersDir);
  if(gen.isEmpty() || dir.isEmpty())
    SKIP(lib::skip_reason(gen.isEmpty() ? generator : viewersDir));

  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const auto viewers = QDir{dir}.entryList({"*.fs"}, QDir::Files, QDir::Name);
  REQUIRE(viewers.size() >= 9);
  for(const auto& v : viewers)
  {
    CAPTURE(v.toStdString());
    IsfResult r;
    score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
      lib::RootGuard guard{ctx};
      r = score::test::gfx::render_isf_chain(backend, {gen, QDir{dir}.filePath(v)});
    });
    if(r.skipped)
      SKIP(r.backend + ": " + r.skip_reason);
    // After a render, like CsfImage3d: it needs the application to exist.
    if(const char* why = score::test::gfx::compute_shader_skip_reason(backend))
      SKIP(why);
    REQUIRE(r.error.empty());
    REQUIRE(r.outputs.size() == 1);
    const auto& img = r.outputs[0];
    REQUIRE(img.valid());
    const auto c = img.center();
    const int distinct = distinct_samples(img);
    INFO("centre=(" << int(c[0]) << "," << int(c[1]) << "," << int(c[2]) << ","
                    << int(c[3]) << ") distinct=" << distinct);
    CHECK(distinct > 2);
  }
}
