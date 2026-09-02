// Coverage for Threedim/GeometryLoader.cpp — the geom_t file port's
// extension dispatch, the four parsers it fans out to (TinyObj, Ply/miniply,
// VcgImporters STL + OFF) and rebuild_geometry(), which turns the flat
// Threedim::mesh record into the halp::dynamic_geometry the rest of the graph
// consumes.
//
// The entry point under test is the same static the avnd runtime calls:
//   GeometryLoader::ins::geom_t::process(file_type)
// It returns a std::function that the execution thread applies to the loader
// instance; an empty function means "parse failed, keep whatever geometry is
// already there". Every "must be rejected" case below asserts exactly that.
//
// Fixtures are written by the test, and each malformed one is derived from the
// well-formed fixture next to it (truncation, or one rewritten field), so this
// file carries its own corpus.

#include "ForkProbe.hpp"

#include <Threedim/GeometryLoader.hpp>
#include <Threedim/Ply.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <random>
#include <string>
#include <vector>

using Catch::Approx;

namespace
{
// One tag per process so a fixture cannot collide with a concurrently running
// copy of this executable.
const std::string& uniqueTag()
{
  static const std::string tag = std::to_string(std::random_device{}());
  return tag;
}

struct TempFile
{
  std::filesystem::path path;
  std::string name;

  TempFile(std::string_view contents, const char* suffix)
  {
    static int counter = 0;
    path = std::filesystem::temp_directory_path()
           / ("score-threedim-geom-" + uniqueTag() + "-"
              + std::to_string(counter++) + suffix);
    std::ofstream f(path, std::ios::binary);
    f.write(contents.data(), std::streamsize(contents.size()));
    f.close();
    name = path.string();
  }
  ~TempFile()
  {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
};

// Drives the shipped dispatch exactly as the runtime does.
bool load(Threedim::GeometryLoader& loader, std::string_view filename,
          std::string_view bytes = {})
{
  halp::text_file_view tv;
  tv.filename = filename;
  tv.bytes = bytes;
  auto fn = Threedim::GeometryLoader::ins::geom_t::process(tv);
  if(!fn)
    return false;
  fn(loader);
  return true;
}

int attributeIndex(
    const halp::dynamic_geometry& g, halp::attribute_semantic sem)
{
  for(std::size_t i = 0; i < g.attributes.size(); i++)
    if(g.attributes[i].semantic == sem)
      return int(i);
  return -1;
}

constexpr std::string_view triangle_obj = R"(v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)";

constexpr std::string_view textured_obj = R"(v 0 0 0
v 1 0 0
v 0 1 0
vt 0 0
vt 1 0
vt 0 1
vn 0 0 1
f 1/1/1 2/2/1 3/3/1
)";

const std::string ascii_ply = R"(ply
format ascii 1.0
element vertex 3
property float x
property float y
property float z
end_header
0 0 0
1 0 0
0 1 0
)";

const std::string rich_ply = R"(ply
format ascii 1.0
element vertex 2
property float x
property float y
property float z
property float nx
property float ny
property float nz
property uchar red
property uchar green
property uchar blue
property float scale_x
property float scale_y
property float scale_z
end_header
0 0 0 0 0 1 255 0 0 1 2 3
1 2 3 0 1 0 0 255 0 4 5 6
)";

const std::string ascii_stl = R"(solid t
facet normal 0 0 1
  outer loop
    vertex 0 0 0
    vertex 1 0 0
    vertex 0 1 0
  endloop
endfacet
facet normal 0 0 1
  outer loop
    vertex 1 0 0
    vertex 1 1 0
    vertex 0 1 0
  endloop
endfacet
endsolid t
)";

const std::string ascii_off = R"(OFF
4 2 0
0 0 0
1 0 0
1 1 0
0 1 0
3 0 1 2
3 0 2 3
)";

const std::string colored_off = R"(COFF
3 1 0
0 0 0 255 0 0 255
1 0 0 0 255 0 255
0 1 0 0 0 255 255
3 0 1 2
)";
} // namespace

TEST_CASE("OBJ bytes reach the loader and publish a position stream",
          "[threedim][geomloader][obj]")
{
  Threedim::GeometryLoader loader;
  REQUIRE(load(loader, "tri.obj", triangle_obj));

  REQUIRE(loader.outputs.geometry.mesh.size() == 1);
  const auto& g = loader.outputs.geometry.mesh[0];
  CHECK(loader.outputs.geometry.dirty_mesh);
  CHECK(g.vertices == 3);
  CHECK(g.topology == halp::primitive_topology::triangles);
  CHECK(g.cull_mode == halp::cull_mode::back);
  CHECK(g.index.buffer == -1);

  REQUIRE(g.buffers.size() == 1);
  CHECK(g.buffers[0].raw_data == loader.complete.data());
  CHECK(g.buffers[0].byte_size == int64_t(loader.complete.size() * sizeof(float)));
  CHECK(g.buffers[0].dirty);

  REQUIRE(g.attributes.size() == 1);
  CHECK(g.attributes[0].semantic == halp::attribute_semantic::position);
  CHECK(g.attributes[0].format == halp::attribute_format::float3);
  CHECK(g.attributes[0].binding == 0);
  REQUIRE(g.bindings.size() == 1);
  CHECK(g.bindings[0].stride == 3 * int(sizeof(float)));
  REQUIRE(g.input.size() == 1);
  CHECK(g.input[0].buffer == 0);
  CHECK(g.input[0].byte_offset == 0);

  const float* p = loader.complete.data();
  CHECK(p[0] == Approx(0.f));
  CHECK(p[3] == Approx(1.f));
  CHECK(p[7] == Approx(1.f));
}

TEST_CASE("an OBJ with UVs and normals publishes four attribute streams",
          "[threedim][geomloader][obj]")
{
  Threedim::GeometryLoader loader;
  REQUIRE(load(loader, "tex.obj", textured_obj));

  REQUIRE(loader.outputs.geometry.mesh.size() == 1);
  const auto& g = loader.outputs.geometry.mesh[0];
  REQUIRE(g.vertices == 3);

  // TinyObj derives tangents whenever both UVs and normals are present, so
  // the published set is position / texcoord0 / normal / tangent.
  REQUIRE(g.attributes.size() == 4);
  REQUIRE(g.bindings.size() == 4);
  REQUIRE(g.input.size() == 4);
  CHECK(g.attributes[0].semantic == halp::attribute_semantic::position);
  CHECK(g.attributes[1].semantic == halp::attribute_semantic::texcoord0);
  CHECK(g.attributes[2].semantic == halp::attribute_semantic::normal);
  CHECK(g.attributes[3].semantic == halp::attribute_semantic::tangent);
  CHECK(g.attributes[3].format == halp::attribute_format::float4);

  for(int i = 0; i < 4; i++)
    CHECK(g.attributes[i].binding == i);

  // Non-interleaved blocks in TinyObj's order: pos | uv | normal | tangent.
  const int64_t v = 3;
  CHECK(g.bindings[0].stride == 3 * int(sizeof(float)));
  CHECK(g.bindings[1].stride == 2 * int(sizeof(float)));
  CHECK(g.bindings[2].stride == 3 * int(sizeof(float)));
  CHECK(g.bindings[3].stride == 4 * int(sizeof(float)));
  CHECK(g.input[0].byte_offset == 0);
  CHECK(g.input[1].byte_offset == v * 3 * int(sizeof(float)));
  CHECK(g.input[2].byte_offset == v * 5 * int(sizeof(float)));
  CHECK(g.input[3].byte_offset == v * 8 * int(sizeof(float)));
  CHECK(loader.complete.size() == std::size_t(v) * (3 + 2 + 3 + 4));
}

TEST_CASE("an ASCII PLY point cloud loads", "[threedim][geomloader][ply]")
{
  TempFile f{ascii_ply, ".ply"};
  Threedim::GeometryLoader loader;
  REQUIRE(load(loader, f.name));

  REQUIRE(loader.outputs.geometry.mesh.size() == 1);
  const auto& g = loader.outputs.geometry.mesh[0];
  CHECK(g.vertices == 3);
  // PlyFromFile always publishes a point cloud (no face element support).
  CHECK(g.topology == halp::primitive_topology::points);
  CHECK(g.cull_mode == halp::cull_mode::none);
  REQUIRE(g.attributes.size() == 1);
  CHECK(g.attributes[0].semantic == halp::attribute_semantic::position);
  CHECK(loader.complete.size() == 9);
}

TEST_CASE("PLY normals, colours and a recognised property group all arrive",
          "[threedim][geomloader][ply]")
{
  TempFile f{rich_ply, ".ply"};
  Threedim::float_vec buf;
  auto meshes = Threedim::PlyFromFile(f.name, buf);
  REQUIRE(meshes.size() == 1);

  const auto& m = meshes[0];
  CHECK(m.vertices == 2);
  CHECK(m.points);
  CHECK(m.normals);
  CHECK(m.colors);
  CHECK_FALSE(m.texcoord);

  // Blocks are laid out in extraction order: pos | uv | normal | colour |
  // extras. There is no uv here.
  CHECK(m.pos_offset == 0);
  CHECK(m.normal_offset == 6);
  CHECK(m.color_offset == 12);

  REQUIRE(buf.size() == 2 * (3 + 3 + 3 + 3));
  CHECK(buf[0] == Approx(0.f));
  CHECK(buf[3] == Approx(1.f));
  CHECK(buf[4] == Approx(2.f));
  CHECK(buf[5] == Approx(3.f));

  // uchar colours are normalised to 0..1.
  CHECK(buf[12] == Approx(1.f));
  CHECK(buf[13] == Approx(0.f));
  CHECK(buf[16] == Approx(1.f));

  // scale_x/y/z is a known group, mapped to one float3 `scale` attribute.
  REQUIRE(m.extras.size() == 1);
  CHECK(m.extras[0].semantic == halp::attribute_semantic::scale);
  CHECK(m.extras[0].format == halp::attribute_format::float3);
  CHECK(m.extras[0].components == 3);
  CHECK(m.extras[0].offset == 18);
  CHECK(buf[18] == Approx(1.f));
  CHECK(buf[21] == Approx(4.f));

  // ...and rebuild_geometry turns each extra into its own binding/attribute.
  Threedim::GeometryLoader loader;
  REQUIRE(load(loader, f.name));
  REQUIRE(loader.outputs.geometry.mesh.size() == 1);
  const auto& g = loader.outputs.geometry.mesh[0];
  REQUIRE(g.attributes.size() == 4);
  CHECK(attributeIndex(g, halp::attribute_semantic::position) == 0);
  CHECK(attributeIndex(g, halp::attribute_semantic::normal) == 1);
  CHECK(attributeIndex(g, halp::attribute_semantic::color0) == 2);
  CHECK(attributeIndex(g, halp::attribute_semantic::scale) == 3);
  REQUIRE(g.bindings.size() == 4);
  REQUIRE(g.input.size() == 4);
  CHECK(g.bindings[3].stride == 3 * int(sizeof(float)));
  CHECK(g.input[3].byte_offset == 18 * int(sizeof(float)));
}

TEST_CASE("an ASCII STL loads as de-indexed triangles",
          "[threedim][geomloader][stl]")
{
  TempFile f{ascii_stl, ".stl"};
  Threedim::GeometryLoader loader;
  REQUIRE(load(loader, f.name));

  REQUIRE(loader.outputs.geometry.mesh.size() == 1);
  const auto& g = loader.outputs.geometry.mesh[0];
  // 2 facets, one output vertex per triangle corner.
  CHECK(g.vertices == 6);
  CHECK(g.topology == halp::primitive_topology::triangles);
  // STL is 'a normal and three vertices per facet': since 1a02c5cabf the
  // openStl wrapper recomputes per-face normals from the winding (vcglib's
  // import_stl.h reads the stored normal and drops it, and never sets
  // IOM_FACENORMAL itself) — so positions AND normals come through.
  REQUIRE(g.attributes.size() == 2);
  CHECK(g.attributes[0].semantic == halp::attribute_semantic::position);
  CHECK(g.attributes[1].semantic == halp::attribute_semantic::normal);
  CHECK(loader.complete.size() == 6 * (3 + 3));
}

TEST_CASE("an ASCII OFF loads as de-indexed triangles",
          "[threedim][geomloader][off]")
{
  TempFile f{ascii_off, ".off"};
  Threedim::GeometryLoader loader;
  REQUIRE(load(loader, f.name));

  REQUIRE(loader.outputs.geometry.mesh.size() == 1);
  const auto& g = loader.outputs.geometry.mesh[0];
  CHECK(g.vertices == 6);
  CHECK(g.topology == halp::primitive_topology::triangles);
  REQUIRE(g.attributes.size() == 1);
  CHECK(loader.complete.size() == 6 * 3);
}

TEST_CASE(
    "a coloured OFF describes its colour stream with the stride it wrote",
    "[threedim][geomloader][off]")
{
  // DEFECT: VcgImporters::convertVcgToMeshes writes colours as RGBA — four
  // floats per corner — but rebuild_geometry (in both GeometryLoader.cpp and
  // VoxelLoader.cpp) hardcodes every colour stream as float3 with a 12-byte
  // stride. For an STL/OFF asset the published descriptor therefore walks the
  // colour block 4 bytes short per vertex, so every vertex after the first
  // samples a shifted, wrong colour. (PLY is unaffected: PlyFromFile writes 3
  // floats per vertex, which is what the descriptor claims.)
  //
  // Either the importer should emit float3, or the descriptor should say
  // float4 / stride 16. This asserts the latter.
  TempFile f{colored_off, ".off"};
  Threedim::GeometryLoader loader;
  REQUIRE(load(loader, f.name));

  REQUIRE(loader.outputs.geometry.mesh.size() == 1);
  const auto& g = loader.outputs.geometry.mesh[0];
  REQUIRE(g.vertices == 3);
  // pos(3) + rgba(4) per corner.
  REQUIRE(loader.complete.size() == 3 * (3 + 4));

  const int ci = attributeIndex(g, halp::attribute_semantic::color0);
  REQUIRE(ci >= 0);
  CHECK(g.attributes[ci].format == halp::attribute_format::float4);
  CHECK(g.bindings[g.attributes[ci].binding].stride == 4 * int(sizeof(float)));
}

TEST_CASE("extension dispatch is case-insensitive", "[threedim][geomloader]")
{
  TempFile f{ascii_ply, ".ply"};
  // The dispatcher compares the tail of the *filename*, not the real path, so
  // an upper-case name for the same file must still take the PLY branch.
  Threedim::GeometryLoader loader;
  REQUIRE(load(loader, f.name));
  const auto vertices = loader.outputs.geometry.mesh.at(0).vertices;

  auto upper = f.path;
  upper.replace_extension(".PLY");
  std::filesystem::copy_file(
      f.path, upper, std::filesystem::copy_options::overwrite_existing);

  Threedim::GeometryLoader loader2;
  REQUIRE(load(loader2, upper.string()));
  CHECK(loader2.outputs.geometry.mesh.at(0).vertices == vertices);

  std::error_code ec;
  std::filesystem::remove(upper, ec);
}

TEST_CASE("unsupported and missing files are rejected", "[threedim][geomloader]")
{
  Threedim::GeometryLoader loader;

  SECTION("an extension nobody handles")
  {
    CHECK_FALSE(load(loader, "scene.fbx", triangle_obj));
    CHECK_FALSE(load(loader, "scene.gltf", triangle_obj));
    CHECK_FALSE(load(loader, "scene", triangle_obj));
    CHECK_FALSE(load(loader, "", triangle_obj));
  }

  SECTION("a .ply / .stl / .off that is not on disk")
  {
    CHECK_FALSE(load(loader, "/nonexistent/score-threedim/nope.ply"));
    CHECK_FALSE(load(loader, "/nonexistent/score-threedim/nope.stl"));
    CHECK_FALSE(load(loader, "/nonexistent/score-threedim/nope.off"));
  }

  SECTION("empty files")
  {
    TempFile ply{"", ".ply"};
    TempFile stl{"", ".stl"};
    TempFile off{"", ".off"};
    CHECK_FALSE(load(loader, ply.name));
    CHECK_FALSE(load(loader, stl.name));
    CHECK_FALSE(load(loader, off.name));
    CHECK_FALSE(load(loader, "empty.obj", ""));
  }

  // Nothing above may have touched the output.
  CHECK(loader.outputs.geometry.mesh.empty());
}

TEST_CASE("malformed assets are rejected without publishing geometry",
          "[threedim][geomloader][malformed]")
{
  Threedim::GeometryLoader loader;

  SECTION("PLY: header that never ends")
  {
    TempFile f{"ply\nformat ascii 1.0\nelement vertex 3\n", ".ply"};
    CHECK_FALSE(load(loader, f.name));
  }

  SECTION("PLY: declares more rows than it carries")
  {
    std::string s = ascii_ply;
    s.replace(s.find("element vertex 3"), 16, "element vertex 9");
    TempFile f{s, ".ply"};
    CHECK_FALSE(load(loader, f.name));
  }

  SECTION("PLY: zero vertices")
  {
    std::string s = ascii_ply;
    s.replace(s.find("element vertex 3"), 16, "element vertex 0");
    TempFile f{s, ".ply"};
    CHECK_FALSE(load(loader, f.name));
  }

  SECTION("PLY: no position properties at all")
  {
    const std::string s = R"(ply
format ascii 1.0
element vertex 2
property float u
property float v
end_header
0 0
1 1
)";
    TempFile f{s, ".ply"};
    CHECK_FALSE(load(loader, f.name));
  }

  SECTION("PLY: truncated at every prefix length")
  {
    for(std::size_t n = 1; n < ascii_ply.size(); n++)
    {
      TempFile f{std::string_view(ascii_ply).substr(0, n), ".ply"};
      INFO("prefix of " << n << " bytes");
      if(load(loader, f.name))
      {
        // A prefix that still parses must publish a consistent descriptor.
        REQUIRE(loader.outputs.geometry.mesh.size() == 1);
        CHECK(loader.outputs.geometry.mesh[0].vertices > 0);
        loader.outputs.geometry.mesh.clear();
      }
    }
  }

  SECTION("STL: garbage, and a facet with too few vertices")
  {
    TempFile g{"solid\nthis is not a facet\nendsolid\n", ".stl"};
    CHECK_FALSE(load(loader, g.name));

    const std::string s = R"(solid t
facet normal 0 0 1
  outer loop
    vertex 0 0 0
  endloop
endfacet
endsolid t
)";
    TempFile f{s, ".stl"};
    // Whatever the importer decides, it must not publish a mesh whose vertex
    // count outruns the buffer it points at.
    if(load(loader, f.name))
    {
      REQUIRE(loader.outputs.geometry.mesh.size() == 1);
      const auto& g2 = loader.outputs.geometry.mesh[0];
      CHECK(
          int64_t(g2.vertices) * 3 * int64_t(sizeof(float))
          <= g2.buffers[0].byte_size);
      loader.outputs.geometry.mesh.clear();
    }
  }

  SECTION("OFF: counts that do not match the body")
  {
    const std::string s = R"(OFF
9 9 0
0 0 0
1 0 0
0 1 0
3 0 1 2
)";
    TempFile f{s, ".off"};
    CHECK_FALSE(load(loader, f.name));
  }

  SECTION("OFF: missing the OFF keyword")
  {
    const std::string s = "3 1 0\n0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n";
    TempFile f{s, ".off"};
    CHECK_FALSE(load(loader, f.name));
  }

  SECTION("OBJ: binary garbage in the bytes")
  {
    std::string junk;
    for(int i = 0; i < 512; i++)
      junk.push_back(char(i * 37));
    CHECK_FALSE(load(loader, "junk.obj", junk));
  }
}

TEST_CASE("a multi-object OBJ publishes one geometry per shape",
          "[threedim][geomloader][obj]")
{
  constexpr std::string_view two = R"(o a
v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
o b
v 2 0 0
v 3 0 0
v 2 1 0
f 4 5 6
)";
  Threedim::GeometryLoader loader;
  REQUIRE(load(loader, "two.obj", two));

  REQUIRE(loader.outputs.geometry.mesh.size() == 2);
  for(auto& g : loader.outputs.geometry.mesh)
  {
    CHECK(g.vertices == 3);
    REQUIRE(g.buffers.size() == 1);
    // Both shapes share one flat buffer and differ only by the input offset.
    CHECK(g.buffers[0].raw_data == loader.complete.data());
  }
  CHECK(loader.outputs.geometry.mesh[0].input[0].byte_offset == 0);
  CHECK(
      loader.outputs.geometry.mesh[1].input[0].byte_offset
      == 3 * 3 * int(sizeof(float)));
}

TEST_CASE("rebuild_geometry drops meshes with no vertices",
          "[threedim][geomloader]")
{
  Threedim::GeometryLoader loader;
  loader.meshinfo = {
      Threedim::mesh{.vertices = 0}, Threedim::mesh{.vertices = 3},
      Threedim::mesh{.vertices = -1}};
  loader.complete.resize(9, 0.f);
  loader.rebuild_geometry();
  CHECK(loader.outputs.geometry.mesh.size() == 1);
  CHECK(loader.outputs.geometry.mesh[0].vertices == 3);
}

TEST_CASE("GaussianSplatsFromPly fills the documented 64-float layout",
          "[threedim][splat]")
{
  const std::string splat_ply = R"(ply
format ascii 1.0
element vertex 2
property float x
property float y
property float z
property float nx
property float ny
property float nz
property float f_dc_0
property float f_dc_1
property float f_dc_2
property float opacity
property float scale_0
property float scale_1
property float scale_2
property float rot_0
property float rot_1
property float rot_2
property float rot_3
end_header
1 2 3 0 0 1 0.1 0.2 0.3 -1.5 -2 -3 -4 1 0 0 0
4 5 6 0 1 0 0.4 0.5 0.6 2.5 -1 -2 -3 0 1 0 0
)";
  TempFile f{splat_ply, ".ply"};

  auto data = Threedim::GaussianSplatsFromPly(f.name);
  REQUIRE(data.splatCount == 2);
  REQUIRE(data.buffer.size() == 2 * Threedim::GaussianSplatData::floatsPerSplat);
  CHECK(Threedim::GaussianSplatData::bytesPerSplat == 256);
  CHECK(data.shRestCount == 0);

  const float* s0 = data.buffer.data();
  const float* s1 = s0 + Threedim::GaussianSplatData::floatsPerSplat;

  CHECK(s0[0] == Approx(1.f));
  CHECK(s0[2] == Approx(3.f));
  CHECK(s0[5] == Approx(1.f));
  CHECK(s0[6] == Approx(0.1f));
  CHECK(s0[8] == Approx(0.3f));
  CHECK(s0[54] == Approx(-1.5f));
  CHECK(s0[55] == Approx(-2.f));
  CHECK(s0[57] == Approx(-4.f));
  CHECK(s0[58] == Approx(1.f));
  CHECK(s0[61] == Approx(0.f));

  CHECK(s1[0] == Approx(4.f));
  CHECK(s1[54] == Approx(2.5f));
  CHECK(s1[59] == Approx(1.f));

  // Absent SH-rest coefficients and the trailing padding stay zeroed.
  for(int i = 9; i < 54; i++)
    CHECK(s0[i] == 0.f);
  CHECK(s0[62] == 0.f);
  CHECK(s0[63] == 0.f);
}

TEST_CASE("GaussianSplatsFromPly counts the SH-rest coefficients present",
          "[threedim][splat]")
{
  std::string header = R"(ply
format ascii 1.0
element vertex 1
property float x
property float y
property float z
)";
  std::string row = "1 2 3";
  for(int i = 0; i < 9; i++)
  {
    header += "property float f_rest_" + std::to_string(i) + "\n";
    row += " " + std::to_string(i + 1);
  }
  header += "end_header\n" + row + "\n";

  TempFile f{header, ".ply"};
  auto data = Threedim::GaussianSplatsFromPly(f.name);
  REQUIRE(data.splatCount == 1);
  CHECK(data.shRestCount == 9);
  for(int i = 0; i < 9; i++)
    CHECK(data.buffer[9 + i] == Approx(float(i + 1)));
  // The 45-slot region is zero past the 9 that exist.
  for(int i = 9 + 9; i < 54; i++)
    CHECK(data.buffer[i] == 0.f);
}

TEST_CASE("GaussianSplatsFromPly rejects bad input without crashing",
          "[threedim][splat][malformed]")
{
  CHECK(Threedim::GaussianSplatsFromPly("/nonexistent/score-threedim/x.ply")
            .splatCount
        == 0);

  TempFile empty{"", ".ply"};
  CHECK(Threedim::GaussianSplatsFromPly(empty.name).splatCount == 0);

  TempFile junk{"ply\nformat ascii 1.0\nelement vertex 4\n", ".ply"};
  CHECK(Threedim::GaussianSplatsFromPly(junk.name).splatCount == 0);

  // A vertex element with none of the splat properties: nothing to extract,
  // but the buffer must still be allocated and zeroed rather than left short.
  TempFile plain{ascii_ply, ".ply"};
  auto data = Threedim::GaussianSplatsFromPly(plain.name);
  CHECK(data.splatCount == 3);
  REQUIRE(data.buffer.size() == 3 * Threedim::GaussianSplatData::floatsPerSplat);
  CHECK(data.shRestCount == 0);
  CHECK(data.buffer[0] == Approx(0.f));
}

#if defined(THREEDIM_HAS_FORK)
TEST_CASE(
    "an OFF face index outside the vertex array must be rejected",
    "[threedim][geomloader][off][malformed]")
{
  // DEFECT (memory safety). vcglib's ImporterOFF passes the face's vertex
  // indices straight to Allocator<>::AddFace, which only asserts them:
  //   assert(v2>=0 && v2<m.vert.size())
  // A .off whose face list names a vertex that does not exist therefore
  // aborts score on any build without NDEBUG, and on a release build builds a
  // face holding a pointer past the end of the vertex vector — which
  // convertVcgToMeshes then dereferences (f.cV(c)->cP()).
  //
  // GeometryLoader has no way to pre-validate this; the range check belongs
  // in the importer wrapper (VcgImporters.cpp) or must be taken upstream.
  const std::string off = R"(OFF
3 1 0
0 0 0
1 0 0
0 1 0
3 0 1 99
)";
  TempFile f{off, ".off"};
  const auto name = f.name;
  CHECK(threedim_test::survives([&] {
    Threedim::GeometryLoader loader;
    load(loader, name);
  }));
}
#endif
