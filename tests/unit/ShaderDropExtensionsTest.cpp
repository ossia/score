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
