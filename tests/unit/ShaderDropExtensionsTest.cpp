// Which shader files each drop handler claims (shader board F01).
//
// `.glsl` is the geometry-filter extension, and the extension of the shared
// includes (score_pbr_brdf.glsl, depth_helpers.glsl). The ISF drop handler
// used to claim it too, but an ISF process only ever loads `.fs` / `.frag`:
// a dropped `.glsl` became an ISF process that never compiled anything.
#include <Gfx/Filter/Library.hpp>
#include <Gfx/GeometryFilter/Library.hpp>
#include <Gfx/ShaderProgram.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QTemporaryDir>

namespace
{
QString write(const QTemporaryDir& dir, const QString& name, const QByteArray& text)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(text);
  return path;
}
}

TEST_CASE("the ISF drop handler does not claim .glsl", "[gfx][isf][F01]")
{
  const Gfx::Filter::DropHandler isf;
  const auto ext = static_cast<const Process::ProcessDropHandler&>(isf).fileExtensions();
  CHECK(ext.contains("fs"));
  CHECK(ext.contains("frag"));
  CHECK_FALSE(ext.contains("glsl"));
}

TEST_CASE("the geometry filter drop handler keeps .glsl", "[gfx][geometry-filter][F01]")
{
  const Gfx::GeometryFilter::DropHandler geo;
  CHECK(static_cast<const Process::ProcessDropHandler&>(geo).fileExtensions().contains(
      "glsl"));
}

TEST_CASE(
    "a .glsl include is no shader family; a geometry filter is", "[gfx][F01]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto include = write(
      dir, "score_helpers.glsl", "float helper(float x) { return x * 2.0; }\n");
  const auto filter = write(
      dir, "Displace.glsl",
      R"_(/*{ "ISFVSN": "2", "MODE": "GEOMETRY_FILTER", "INPUTS": [] }*/
void process_vertex(inout vec3 position) { }
)_");
  CHECK(Gfx::shaderFileFamily(include) == Gfx::ShaderFamily::Unknown);
  CHECK(Gfx::shaderFileFamily(filter) == Gfx::ShaderFamily::GeometryFilter);
}

// The family is the header's MODE, read case-independently (U02); the shader
// body is not looked at. The whole file was upper-cased and searched, for
// every library file the browser lists: a body that names another mode in a
// comment or a string took that family.
TEST_CASE("a shader's family comes from its header, not its body", "[gfx][U02]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto isf = write(
      dir, "Blur.fs",
      R"_(/*{ "ISFVSN": "2", "INPUTS": [] }*/
// Not a "COMPUTE_SHADER": see the "RAW_RASTER_PIPELINE" variant.
void main() { gl_FragColor = vec4(1.0); }
)_");
  CHECK(Gfx::shaderFileFamily(isf) == Gfx::ShaderFamily::Unknown);
  const auto lower = write(
      dir, "Comp.cs",
      "// licence\n/* { \"isfvsn\": \"2\", \"mode\": \"compute_shader\" } */\nvoid main() {}\n");
  CHECK(Gfx::shaderFileFamily(lower) == Gfx::ShaderFamily::Compute);
  const auto licence = write(
      dir, "Raster.frag",
      "/* (c) 2024, \"GEOMETRY_FILTER\" is not this */\n/*{ \"MODE\": \"RAW_RASTER_PIPELINE\" }*/\nvoid main() {}\n");
  CHECK(Gfx::shaderFileFamily(licence) == Gfx::ShaderFamily::RawRaster);
}
