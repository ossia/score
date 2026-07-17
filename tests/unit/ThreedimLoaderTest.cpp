// Unit tests for the threedim 3D-asset loading layer:
//
//  - Threedim::ObjFromString            (tinyobjloader bridge, TinyObj.cpp)
//  - Threedim::sceneStateFromMeshes     (SceneFromMeshes.cpp)
//  - Threedim::GltfParser               (fastgltf bridge, GltfParser.cpp)
//  - Threedim::FbxParser                (ufbx bridge, FbxParser.cpp)
//  - Threedim::AssetLoader dispatch     (AssetLoader.cpp) + AssetLoaderRegistry
//
// The parsers are synchronous, static entry points (`ins::xxx_t::process`
// returns an apply-lambda), so everything here runs without any Qt event
// loop, RHI, or application context.
//
// glTF / GLB / FBX fixtures are generated in a temp dir by the test itself
// (single triangle with known positions / normals / UVs / indices, plus a
// base64-embedded buffer for .gltf and a binary chunk for .glb).
// OBJ fixtures come from ~/Documents/ModelsOBJ (cube.obj: 8 verts / 12 tris,
// sphere20.obj: 252 verts / 500 tris, both WITHOUT vn normals); those
// sections are SKIPped when the corpus is absent.
//
// Documented loader behaviours asserted below:
//  * OBJ is expanded to an UNINDEXED triangle soup: 12 tris -> 36 vertices,
//    index_format::none. Vertex count is faces*3, not the "v" line count.
//  * An OBJ without "vn" lines yields NO normal attribute — the OBJ path
//    does NOT generate normals (mesh.normals == false, position is the
//    only attribute).
//  * glTF keeps indexed geometry; indices are widened to uint32.
//  * A glTF with normals+UVs but no TANGENT gets tangents generated via
//    mikktspace; without normals/UVs nothing is generated.
//  * FBX (ufbx) DOES generate missing normals (generate_missing_normals).
//  * Corrupt / truncated / OOB-index inputs must return a null result
//    (empty std::function) without crashing (ASAN-clean).

#include <Threedim/AssetLoader.hpp>
#include <Threedim/FbxParser.hpp>
#include <Threedim/GltfParser.hpp>
#include <Threedim/SceneFromMeshes.hpp>
#include <Threedim/TinyObj.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QCoreApplication>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Catch::Approx;

namespace
{

// ---------------------------------------------------------------------------
// Fixture helpers
// ---------------------------------------------------------------------------

struct TempDir
{
  fs::path dir;
  TempDir()
  {
    auto base = fs::temp_directory_path();
    dir = base
          / ("threedim-loader-test-" + std::to_string(uint64_t(QCoreApplication::applicationPid())));
    fs::create_directories(dir);
  }
  ~TempDir()
  {
    std::error_code ec;
    fs::remove_all(dir, ec);
  }
  fs::path write(const std::string& name, const std::string& bytes) const
  {
    auto p = dir / name;
    std::ofstream f(p, std::ios::binary);
    f.write(bytes.data(), std::streamsize(bytes.size()));
    return p;
  }
};

static std::optional<std::string> read_file(const fs::path& p)
{
  std::ifstream f(p, std::ios::binary);
  if(!f)
    return std::nullopt;
  std::string s(
      (std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return s;
}

static const fs::path models_dir
    = fs::path{::getenv("HOME") ? ::getenv("HOME") : "/home/jcelerier"}
      / "Documents" / "ModelsOBJ";

static std::string b64(const std::string& in)
{
  static const char tbl[]
      = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((in.size() + 2) / 3) * 4);
  std::size_t i = 0;
  for(; i + 3 <= in.size(); i += 3)
  {
    uint32_t v = (uint8_t(in[i]) << 16) | (uint8_t(in[i + 1]) << 8)
                 | uint8_t(in[i + 2]);
    out.push_back(tbl[(v >> 18) & 63]);
    out.push_back(tbl[(v >> 12) & 63]);
    out.push_back(tbl[(v >> 6) & 63]);
    out.push_back(tbl[v & 63]);
  }
  const auto rem = in.size() - i;
  if(rem == 1)
  {
    uint32_t v = uint8_t(in[i]) << 16;
    out.push_back(tbl[(v >> 18) & 63]);
    out.push_back(tbl[(v >> 12) & 63]);
    out += "==";
  }
  else if(rem == 2)
  {
    uint32_t v = (uint8_t(in[i]) << 16) | (uint8_t(in[i + 1]) << 8);
    out.push_back(tbl[(v >> 18) & 63]);
    out.push_back(tbl[(v >> 12) & 63]);
    out.push_back(tbl[(v >> 6) & 63]);
    out += "=";
  }
  return out;
}

template <typename T>
static void put(std::string& s, T v)
{
  char buf[sizeof(T)];
  std::memcpy(buf, &v, sizeof(T));
  s.append(buf, sizeof(T));
}

// Single triangle, positions (0,0,0) (1,0,0) (0,1,0), normals all (0,0,1),
// uv (0,0) (1,0) (0,1), u16 indices 0,1,2. Layout: pos @0 (36B),
// nrm @36 (36B), uv @72 (24B), idx @96 (6B) => 102 bytes.
static std::string triangle_buffer_bytes()
{
  std::string b;
  const float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  const float nrm[9] = {0, 0, 1, 0, 0, 1, 0, 0, 1};
  const float uv[6] = {0, 0, 1, 0, 0, 1};
  const uint16_t idx[3] = {0, 1, 2};
  for(float f : pos)
    put(b, f);
  for(float f : nrm)
    put(b, f);
  for(float f : uv)
    put(b, f);
  for(uint16_t i : idx)
    put(b, i);
  return b;
}

// `uri_or_empty`: data: URI for .gltf; empty (binary GLB buffer) for .glb.
// `with_normals_uvs`: when false, only POSITION is referenced.
static std::string
triangle_gltf_json(const std::string& uri_or_empty, bool with_normals_uvs)
{
  std::string buffer = R"({"byteLength":102)";
  if(!uri_or_empty.empty())
    buffer += R"(,"uri":")" + uri_or_empty + R"(")";
  buffer += "}";

  std::string attrs = R"("POSITION":0)";
  if(with_normals_uvs)
    attrs += R"(,"NORMAL":1,"TEXCOORD_0":2)";

  // clang-format off
  return std::string{R"({
"asset":{"version":"2.0"},
"scene":0,
"scenes":[{"nodes":[0]}],
"nodes":[{"mesh":0,"name":"tri"}],
"meshes":[{"primitives":[{"attributes":{)"} + attrs + R"(},"indices":3}]}],
"buffers":[)" + buffer + R"(],
"bufferViews":[
 {"buffer":0,"byteOffset":0,"byteLength":96,"target":34962},
 {"buffer":0,"byteOffset":96,"byteLength":6,"target":34963}],
"accessors":[
 {"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
 {"bufferView":0,"byteOffset":36,"componentType":5126,"count":3,"type":"VEC3"},
 {"bufferView":0,"byteOffset":72,"componentType":5126,"count":3,"type":"VEC2"},
 {"bufferView":1,"byteOffset":0,"componentType":5123,"count":3,"type":"SCALAR"}]
})";
  // clang-format on
}

static std::string make_glb(const std::string& json, const std::string& bin)
{
  auto pad4 = [](std::string s, char fill) {
    while(s.size() % 4)
      s.push_back(fill);
    return s;
  };
  const std::string j = pad4(json, ' ');
  const std::string b = pad4(bin, '\0');

  std::string out;
  put<uint32_t>(out, 0x46546C67); // 'glTF'
  put<uint32_t>(out, 2);
  put<uint32_t>(out, uint32_t(12 + 8 + j.size() + 8 + b.size()));
  put<uint32_t>(out, uint32_t(j.size()));
  put<uint32_t>(out, 0x4E4F534A); // 'JSON'
  out += j;
  put<uint32_t>(out, uint32_t(b.size()));
  put<uint32_t>(out, 0x004E4942); // 'BIN\0'
  out += b;
  return out;
}

// Minimal ASCII FBX 7.4 document with a single triangle mesh.
// No normals in the file — FbxParser sets ufbx generate_missing_normals.
static std::string triangle_ascii_fbx()
{
  return R"(; FBX 7.4.0 project file
FBXHeaderExtension:  {
	FBXHeaderVersion: 1003
	FBXVersion: 7400
}
GlobalSettings:  {
	Version: 1000
	Properties70:  {
		P: "UpAxis", "int", "Integer", "",1
		P: "UnitScaleFactor", "double", "Number", "",100
	}
}
Definitions:  {
	Version: 100
	Count: 2
	ObjectType: "Geometry" {
		Count: 1
	}
	ObjectType: "Model" {
		Count: 1
	}
}
Objects:  {
	Geometry: 1000, "Geometry::tri", "Mesh" {
		Vertices: *9 {
			a: 0,0,0,1,0,0,0,1,0
		}
		PolygonVertexIndex: *3 {
			a: 0,1,-3
		}
		GeometryVersion: 124
	}
	Model: 2000, "Model::triangle", "Mesh" {
		Version: 232
	}
}
Connections:  {
	C: "OO",1000,2000
	C: "OO",2000,0
}
)";
}

// ---------------------------------------------------------------------------
// scene_state inspection helpers
// ---------------------------------------------------------------------------

static const ossia::mesh_component*
find_first_mesh(const ossia::scene_node& n)
{
  if(!n.children)
    return nullptr;
  for(const auto& payload : *n.children)
  {
    if(auto* mc = ossia::get_if<ossia::mesh_component_ptr>(&payload))
      return mc->get();
    if(auto* child = ossia::get_if<ossia::scene_node_ptr>(&payload))
      if(auto* m = find_first_mesh(**child))
        return m;
  }
  return nullptr;
}

static const ossia::mesh_component* find_first_mesh(const ossia::scene_state& s)
{
  if(!s.roots)
    return nullptr;
  for(const auto& r : *s.roots)
    if(auto* m = find_first_mesh(*r))
      return m;
  return nullptr;
}

static const ossia::vertex_attribute*
find_attr(const ossia::mesh_primitive& p, ossia::attribute_semantic sem)
{
  for(const auto& a : p.attributes)
    if(a.semantic == sem)
      return &a;
  return nullptr;
}

// Resolve the CPU float pointer for an attribute (non-GPU-resident buffers).
static const float*
attr_floats(const ossia::mesh_primitive& p, const ossia::vertex_attribute& a)
{
  REQUIRE(a.buffer_index < p.vertex_buffers.size());
  const auto& br = p.vertex_buffers[a.buffer_index];
  REQUIRE(br);
  const auto* bd = ossia::get_if<ossia::buffer_data>(&br->resource);
  REQUIRE(bd);
  REQUIRE(bd->data);
  return reinterpret_cast<const float*>(
      reinterpret_cast<const char*>(bd->data.get()) + a.byte_offset);
}

} // namespace

// ===========================================================================
// OBJ — tinyobjloader bridge
// ===========================================================================

TEST_CASE("OBJ: cube.obj — exact counts and first-triangle values",
          "[threedim][obj]")
{
  const auto data = read_file(models_dir / "cube.obj");
  if(!data)
    SKIP("ModelsOBJ corpus not available");

  Threedim::float_vec buf;
  auto meshes = Threedim::ObjFromString(*data, buf);

  // Single "g main" group -> one mesh.
  REQUIRE(meshes.size() == 1);
  const auto& m = meshes[0];

  // 8 "v" lines / 12 "f" triangles. The loader outputs an UNINDEXED
  // triangle soup: 12 faces x 3 corners = 36 vertices.
  CHECK(m.vertices == 36);

  // cube.obj has no "vn" and no "vt": the loader does NOT generate
  // normals for OBJ — it simply marks them absent.
  CHECK(m.normals == false);
  CHECK(m.texcoord == false);
  CHECK(m.tangents == false);
  CHECK(m.points == false);
  CHECK(m.pos_offset == 0);

  // Positions only: 36 vertices * 3 floats.
  CHECK(buf.size() == 36 * 3);

  // First face is "f 1 2 3":
  //   v1 = (0, 1.632990, -1.154700)
  //   v2 = (1.632990, 0, -1.154700)
  //   v3 = (0, -1.632990, -1.154700)
  CHECK(buf[0] == Approx(0.0f));
  CHECK(buf[1] == Approx(1.632990f));
  CHECK(buf[2] == Approx(-1.154700f));
  CHECK(buf[3] == Approx(1.632990f));
  CHECK(buf[4] == Approx(0.0f));
  CHECK(buf[5] == Approx(-1.154700f));
  CHECK(buf[6] == Approx(0.0f));
  CHECK(buf[7] == Approx(-1.632990f));
  CHECK(buf[8] == Approx(-1.154700f));
}

TEST_CASE("OBJ: sphere20.obj — exact counts", "[threedim][obj]")
{
  const auto data = read_file(models_dir / "sphere20.obj");
  if(!data)
    SKIP("ModelsOBJ corpus not available");

  Threedim::float_vec buf;
  auto meshes = Threedim::ObjFromString(*data, buf);

  REQUIRE(meshes.size() == 1);
  // 252 "v" lines / 500 "f" triangles -> 1500 soup vertices.
  CHECK(meshes[0].vertices == 500 * 3);
  CHECK(meshes[0].normals == false);
  CHECK(meshes[0].texcoord == false);
  CHECK(buf.size() == 500 * 3 * 3);

  // First face "f 1 2 3", v1 = (0, 0, 1).
  CHECK(buf[0] == Approx(0.0f));
  CHECK(buf[1] == Approx(0.0f));
  CHECK(buf[2] == Approx(1.0f));
}

TEST_CASE("OBJ: normals + texcoords -> mikktspace tangents generated",
          "[threedim][obj]")
{
  // Two-triangle quad in the XY plane, full v/vt/vn triplets.
  const std::string obj = R"(v 0 0 0
v 1 0 0
v 1 1 0
v 0 1 0
vt 0 0
vt 1 0
vt 1 1
vt 0 1
vn 0 0 1
f 1/1/1 2/2/1 3/3/1
f 1/1/1 3/3/1 4/4/1
)";
  Threedim::float_vec buf;
  auto meshes = Threedim::ObjFromString(obj, buf);
  REQUIRE(meshes.size() == 1);
  const auto& m = meshes[0];
  CHECK(m.vertices == 6);
  CHECK(m.normals == true);
  CHECK(m.texcoord == true);
  // Tangents are generated iff normals AND texcoords are present.
  CHECK(m.tangents == true);
  // Layout: pos(6*3) + uv(6*2) + nrm(6*3) + tan(6*4).
  CHECK(buf.size() == 6 * (3 + 2 + 3 + 4));
  CHECK(m.pos_offset == 0);
  CHECK(m.texcoord_offset == 18);
  CHECK(m.normal_offset == 30);
  CHECK(m.tangent_offset == 48);

  // Normals copied through.
  CHECK(buf[m.normal_offset + 0] == Approx(0.f));
  CHECK(buf[m.normal_offset + 1] == Approx(0.f));
  CHECK(buf[m.normal_offset + 2] == Approx(1.f));

  // Tangent for this axis-aligned UV mapping is (1,0,0), |w| = 1.
  CHECK(buf[m.tangent_offset + 0] == Approx(1.f));
  CHECK(buf[m.tangent_offset + 1] == Approx(0.f));
  CHECK(buf[m.tangent_offset + 2] == Approx(0.f));
  CHECK(std::abs(buf[m.tangent_offset + 3]) == Approx(1.f));

  // Scene assembly exposes all four attributes.
  auto state = Threedim::sceneStateFromMeshes(
      std::move(meshes), std::move(buf), "quad.obj");
  REQUIRE(state);
  const auto* mesh = find_first_mesh(*state);
  REQUIRE(mesh);
  const auto& prim = mesh->primitives[0];
  CHECK(prim.attributes.size() == 4);
  CHECK(find_attr(prim, ossia::attribute_semantic::position));
  CHECK(find_attr(prim, ossia::attribute_semantic::normal));
  CHECK(find_attr(prim, ossia::attribute_semantic::texcoord0));
  const auto* tan = find_attr(prim, ossia::attribute_semantic::tangent);
  REQUIRE(tan);
  CHECK(tan->format == ossia::vertex_format::float4);
}

TEST_CASE("OBJ: normals-only and texcoords-only variants", "[threedim][obj]")
{
  SECTION("vn only")
  {
    const std::string obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\n"
                            "f 1//1 2//1 3//1\n";
    Threedim::float_vec buf;
    auto meshes = Threedim::ObjFromString(obj, buf);
    REQUIRE(meshes.size() == 1);
    CHECK(meshes[0].vertices == 3);
    CHECK(meshes[0].normals == true);
    CHECK(meshes[0].texcoord == false);
    CHECK(meshes[0].tangents == false); // needs both normals and uvs
    CHECK(buf.size() == 3 * (3 + 3));
    CHECK(buf[meshes[0].normal_offset + 2] == Approx(1.f));
  }
  SECTION("vt only")
  {
    const std::string obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\n"
                            "f 1/1 2/2 3/3\n";
    Threedim::float_vec buf;
    auto meshes = Threedim::ObjFromString(obj, buf);
    REQUIRE(meshes.size() == 1);
    CHECK(meshes[0].vertices == 3);
    CHECK(meshes[0].normals == false);
    CHECK(meshes[0].texcoord == true);
    CHECK(meshes[0].tangents == false);
    CHECK(buf.size() == 3 * (3 + 2));
    CHECK(buf[meshes[0].texcoord_offset + 2] == Approx(1.f));
  }
}

TEST_CASE("OBJ: corrupt / hostile inputs fail gracefully", "[threedim][obj]")
{
  Threedim::float_vec buf;

  SECTION("empty input")
  {
    auto meshes = Threedim::ObjFromString(std::string_view{}, buf);
    CHECK(meshes.empty());
  }
  SECTION("binary garbage")
  {
    const std::string garbage = "\x89PNG\r\n\x1a\n definitely not an obj \0\1\2";
    auto meshes = Threedim::ObjFromString(garbage, buf);
    CHECK(meshes.empty());
  }
  SECTION("face index out of range (tinyobj does not bounds-check)")
  {
    // One vertex but the face references vertices 1..3: indices 2 and 3
    // are out of range; the loader's explicit range check must bail.
    const std::string obj = "v 0 0 0\nf 1 2 3\n";
    auto meshes = Threedim::ObjFromString(obj, buf);
    CHECK(meshes.empty());
  }
  SECTION("hugely out-of-range face index")
  {
    const std::string obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 2000000000\n";
    auto meshes = Threedim::ObjFromString(obj, buf);
    CHECK(meshes.empty());
  }
  SECTION("negative relative index beyond start")
  {
    const std::string obj = "v 0 0 0\nf -5 -6 -7\n";
    auto meshes = Threedim::ObjFromString(obj, buf);
    CHECK(meshes.empty());
  }
}

TEST_CASE("OBJ -> sceneStateFromMeshes: scene assembly", "[threedim][obj]")
{
  const auto data = read_file(models_dir / "cube.obj");
  if(!data)
    SKIP("ModelsOBJ corpus not available");

  Threedim::float_vec buf;
  auto meshes = Threedim::ObjFromString(*data, buf);
  REQUIRE(meshes.size() == 1);

  auto state = Threedim::sceneStateFromMeshes(
      std::move(meshes), std::move(buf), "cube.obj");
  REQUIRE(state);
  REQUIRE(state->roots);
  REQUIRE(state->roots->size() == 1);

  const auto& node = *(*state->roots)[0];
  CHECK(node.name == "cube.obj");
  CHECK(node.id.value != 0);

  const auto* mesh = find_first_mesh(*state);
  REQUIRE(mesh);
  REQUIRE(mesh->primitives.size() == 1);
  const auto& prim = mesh->primitives[0];

  CHECK(prim.vertex_count == 36);
  CHECK(prim.index_count == 0);
  CHECK(prim.index_type == ossia::index_format::none);
  CHECK(prim.topology == ossia::primitive_topology::triangles);
  CHECK(prim.stable_id != 0);

  // Position is the ONLY attribute (no normals / uvs in the file, none
  // generated by the OBJ path).
  REQUIRE(prim.attributes.size() == 1);
  const auto* pos = find_attr(prim, ossia::attribute_semantic::position);
  REQUIRE(pos);
  CHECK(pos->format == ossia::vertex_format::float3);
  CHECK(pos->byte_offset == 0);
  CHECK(pos->byte_stride == 12);
  CHECK(find_attr(prim, ossia::attribute_semantic::normal) == nullptr);

  // AABB over all 36 soup positions == cube extents.
  CHECK_FALSE(prim.bounds.empty());
  CHECK(prim.bounds.min[0] == Approx(-1.632990f));
  CHECK(prim.bounds.min[1] == Approx(-1.632990f));
  CHECK(prim.bounds.min[2] == Approx(-1.154700f));
  CHECK(prim.bounds.max[0] == Approx(1.632990f));
  CHECK(prim.bounds.max[1] == Approx(1.632990f));
  CHECK(prim.bounds.max[2] == Approx(1.154700f));

  // First vertex readable through the shared CPU buffer.
  const float* p = attr_floats(prim, *pos);
  CHECK(p[0] == Approx(0.0f));
  CHECK(p[1] == Approx(1.632990f));
  CHECK(p[2] == Approx(-1.154700f));
}

TEST_CASE("sceneStateFromMeshes: empty input returns null", "[threedim][obj]")
{
  CHECK(Threedim::sceneStateFromMeshes({}, {}, "x") == nullptr);

  // Non-empty meshes but empty buffer -> null too.
  std::vector<Threedim::mesh> ms{Threedim::mesh{.vertices = 3}};
  CHECK(Threedim::sceneStateFromMeshes(std::move(ms), {}, "x") == nullptr);
}

// ===========================================================================
// glTF — fastgltf bridge
// ===========================================================================

static void check_triangle_gltf_scene(const ossia::scene_state& state)
{
  REQUIRE(state.roots);
  REQUIRE(state.roots->size() == 1);
  const auto& node = *(*state.roots)[0];
  CHECK(node.name == "tri");
  // stable_id = glTF node index + 1.
  CHECK(node.id.value == 1);

  const auto* mesh = find_first_mesh(state);
  REQUIRE(mesh);
  REQUIRE(mesh->primitives.size() == 1);
  const auto& prim = mesh->primitives[0];

  CHECK(prim.vertex_count == 3);
  CHECK(prim.index_count == 3);
  // glTF u16 indices are widened to u32 by the parser.
  CHECK(prim.index_type == ossia::index_format::uint32);
  CHECK(prim.topology == ossia::primitive_topology::triangles);

  // position + normal + texcoord0, PLUS a mikktspace-generated tangent
  // (the file ships normals + uvs but no TANGENT).
  const auto* pos = find_attr(prim, ossia::attribute_semantic::position);
  const auto* nrm = find_attr(prim, ossia::attribute_semantic::normal);
  const auto* uv = find_attr(prim, ossia::attribute_semantic::texcoord0);
  const auto* tan = find_attr(prim, ossia::attribute_semantic::tangent);
  REQUIRE(pos);
  REQUIRE(nrm);
  REQUIRE(uv);
  REQUIRE(tan);
  CHECK(prim.attributes.size() == 4);
  CHECK(pos->format == ossia::vertex_format::float3);
  CHECK(nrm->format == ossia::vertex_format::float3);
  CHECK(uv->format == ossia::vertex_format::float2);
  CHECK(tan->format == ossia::vertex_format::float4);

  // Non-interleaved: one buffer per attribute.
  CHECK(prim.vertex_buffers.size() == 4);

  const float* p = attr_floats(prim, *pos);
  CHECK(p[0] == Approx(0.f));
  CHECK(p[1] == Approx(0.f));
  CHECK(p[2] == Approx(0.f));
  CHECK(p[3] == Approx(1.f));
  CHECK(p[4] == Approx(0.f));
  CHECK(p[5] == Approx(0.f));
  CHECK(p[6] == Approx(0.f));
  CHECK(p[7] == Approx(1.f));
  CHECK(p[8] == Approx(0.f));

  const float* n = attr_floats(prim, *nrm);
  for(int i = 0; i < 3; i++)
  {
    CHECK(n[i * 3 + 0] == Approx(0.f));
    CHECK(n[i * 3 + 1] == Approx(0.f));
    CHECK(n[i * 3 + 2] == Approx(1.f));
  }

  const float* t = attr_floats(prim, *uv);
  CHECK(t[0] == Approx(0.f));
  CHECK(t[1] == Approx(0.f));
  CHECK(t[2] == Approx(1.f));
  CHECK(t[3] == Approx(0.f));
  CHECK(t[4] == Approx(0.f));
  CHECK(t[5] == Approx(1.f));

  // Tangent: mikktspace on this flat +Z triangle with identity-ish UVs
  // gives (1, 0, 0, w). Handedness must be +-1.
  const float* tg = attr_floats(prim, *tan);
  CHECK(tg[0] == Approx(1.f));
  CHECK(tg[1] == Approx(0.f));
  CHECK(tg[2] == Approx(0.f));
  CHECK(std::abs(tg[3]) == Approx(1.f));

  // Index buffer content 0,1,2 as u32.
  REQUIRE(prim.index_buffer);
  const auto* ibd = ossia::get_if<ossia::buffer_data>(&prim.index_buffer->resource);
  REQUIRE(ibd);
  REQUIRE(ibd->byte_size == 3 * int64_t(sizeof(uint32_t)));
  const uint32_t* idx = reinterpret_cast<const uint32_t*>(ibd->data.get());
  CHECK(idx[0] == 0);
  CHECK(idx[1] == 1);
  CHECK(idx[2] == 2);

  // AABB from POSITION stream.
  CHECK(prim.bounds.min[0] == Approx(0.f));
  CHECK(prim.bounds.min[1] == Approx(0.f));
  CHECK(prim.bounds.min[2] == Approx(0.f));
  CHECK(prim.bounds.max[0] == Approx(1.f));
  CHECK(prim.bounds.max[1] == Approx(1.f));
  CHECK(prim.bounds.max[2] == Approx(0.f));
}

TEST_CASE("glTF: minimal triangle .gltf with base64-embedded buffer",
          "[threedim][gltf]")
{
  TempDir tmp;
  const auto bin = triangle_buffer_bytes();
  REQUIRE(bin.size() == 102);
  const auto json = triangle_gltf_json(
      "data:application/octet-stream;base64," + b64(bin), true);
  const auto path = tmp.write("tri.gltf", json);

  const std::string pathstr = path.string();
  const auto contents = read_file(path);
  REQUIRE(contents);

  auto apply = Threedim::GltfParser::ins::gltf_t::process(
      halp::text_file_view{.bytes = *contents, .filename = pathstr});
  REQUIRE(apply);

  Threedim::GltfParser parser;
  apply(parser);
  REQUIRE(parser.m_raw_state);
  check_triangle_gltf_scene(*parser.m_raw_state);
}

TEST_CASE("glTF: .glb binary container variant", "[threedim][gltf]")
{
  TempDir tmp;
  const auto bin = triangle_buffer_bytes();
  const auto json = triangle_gltf_json({}, true); // buffer 0 = GLB BIN chunk
  const auto glb = make_glb(json, bin);
  const auto path = tmp.write("tri.glb", glb);

  const std::string pathstr = path.string();
  const auto contents = read_file(path);
  REQUIRE(contents);

  auto apply = Threedim::GltfParser::ins::gltf_t::process(
      halp::text_file_view{.bytes = *contents, .filename = pathstr});
  REQUIRE(apply);

  Threedim::GltfParser parser;
  apply(parser);
  REQUIRE(parser.m_raw_state);
  check_triangle_gltf_scene(*parser.m_raw_state);
}

TEST_CASE("glTF: positions-only file -> no normals, no tangents",
          "[threedim][gltf]")
{
  TempDir tmp;
  const auto bin = triangle_buffer_bytes();
  const auto json = triangle_gltf_json(
      "data:application/octet-stream;base64," + b64(bin), false);
  const auto path = tmp.write("tri-pos.gltf", json);

  const std::string pathstr = path.string();
  auto apply = Threedim::GltfParser::ins::gltf_t::process(
      halp::text_file_view{.bytes = {}, .filename = pathstr});
  REQUIRE(apply);

  Threedim::GltfParser parser;
  apply(parser);
  REQUIRE(parser.m_raw_state);

  const auto* mesh = find_first_mesh(*parser.m_raw_state);
  REQUIRE(mesh);
  const auto& prim = mesh->primitives[0];
  CHECK(prim.vertex_count == 3);
  // The glTF path does NOT generate normals; and tangent generation
  // requires normals + uvs, so neither appears.
  CHECK(find_attr(prim, ossia::attribute_semantic::position) != nullptr);
  CHECK(find_attr(prim, ossia::attribute_semantic::normal) == nullptr);
  CHECK(find_attr(prim, ossia::attribute_semantic::tangent) == nullptr);
  CHECK(prim.attributes.size() == 1);
}

TEST_CASE("glTF: materials, vertex colors, node hierarchy, camera, light",
          "[threedim][gltf]")
{
  TempDir tmp;

  // Buffer: pos (36B @0), COLOR_0 vec3 (36B @36), u16 idx (6B @72) = 78B.
  std::string bin;
  const float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  const float col[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  const uint16_t idx[3] = {0, 1, 2};
  for(float f : pos)
    put(bin, f);
  for(float f : col)
    put(bin, f);
  for(uint16_t i : idx)
    put(bin, i);
  REQUIRE(bin.size() == 78);

  // clang-format off
  const std::string json = std::string{R"({
"asset":{"version":"2.0"},
"extensionsUsed":["KHR_lights_punctual"],
"extensions":{"KHR_lights_punctual":{"lights":[
  {"type":"point","color":[1.0,0.5,0.25],"intensity":3.0}]}},
"scene":0,
"scenes":[{"nodes":[0]}],
"nodes":[
 {"mesh":0,"name":"parent","translation":[1,2,3],"children":[1,2]},
 {"name":"camchild","camera":0},
 {"name":"lightchild","extensions":{"KHR_lights_punctual":{"light":0}}}],
"cameras":[{"type":"perspective","perspective":{"yfov":1.0,"znear":0.01,"zfar":100.0}}],
"materials":[{"pbrMetallicRoughness":{
  "baseColorFactor":[0.9,0.5,0.25,1.0],
  "metallicFactor":0.1,"roughnessFactor":0.8}}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"COLOR_0":1},"indices":2,"material":0}]}],
"buffers":[{"byteLength":78,"uri":"data:application/octet-stream;base64,)"}
    + b64(bin) + R"("}],
"bufferViews":[
 {"buffer":0,"byteOffset":0,"byteLength":72,"target":34962},
 {"buffer":0,"byteOffset":72,"byteLength":6,"target":34963}],
"accessors":[
 {"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
 {"bufferView":0,"byteOffset":36,"componentType":5126,"count":3,"type":"VEC3"},
 {"bufferView":1,"byteOffset":0,"componentType":5123,"count":3,"type":"SCALAR"}]
})";
  // clang-format on

  const auto path = tmp.write("rich.gltf", json);
  const std::string pathstr = path.string();

  auto apply = Threedim::GltfParser::ins::gltf_t::process(
      halp::text_file_view{.bytes = {}, .filename = pathstr});
  REQUIRE(apply);
  Threedim::GltfParser parser;
  apply(parser);
  REQUIRE(parser.m_raw_state);
  const auto& state = *parser.m_raw_state;

  // Materials surfaced at scene scope AND wired to the primitive.
  REQUIRE(state.materials);
  REQUIRE(state.materials->size() == 1);
  const auto* mesh = find_first_mesh(state);
  REQUIRE(mesh);
  const auto& prim = mesh->primitives[0];
  REQUIRE(prim.material);
  CHECK(prim.material->base_color_factor[0] == Approx(0.9f));
  CHECK(prim.material->base_color_factor[1] == Approx(0.5f));
  CHECK(prim.material->base_color_factor[2] == Approx(0.25f));
  CHECK(prim.material->base_color_factor[3] == Approx(1.0f));
  CHECK(prim.material->metallic_factor == Approx(0.1f));
  CHECK(prim.material->roughness_factor == Approx(0.8f));

  // COLOR_0 was VEC3 in the file: parser pads to RGBA with alpha = 1.
  const auto* c0 = find_attr(prim, ossia::attribute_semantic::color0);
  REQUIRE(c0);
  CHECK(c0->format == ossia::vertex_format::float4);
  const float* cols = attr_floats(prim, *c0);
  CHECK(cols[0] == Approx(1.f)); // r of vertex 0
  CHECK(cols[3] == Approx(1.f)); // padded alpha of vertex 0
  CHECK(cols[5] == Approx(1.f)); // g of vertex 1
  CHECK(cols[7] == Approx(1.f)); // padded alpha of vertex 1

  // Hierarchy: root "parent" carries its TRS as the first payload and the
  // two children as scene_node payloads.
  REQUIRE(state.roots);
  REQUIRE(state.roots->size() == 1);
  const auto& root = *(*state.roots)[0];
  CHECK(root.name == "parent");
  REQUIRE(root.children);
  const auto* root_xf
      = ossia::get_if<ossia::scene_transform>(&(*root.children)[0]);
  REQUIRE(root_xf);
  CHECK(root_xf->translation[0] == Approx(1.f));
  CHECK(root_xf->translation[1] == Approx(2.f));
  CHECK(root_xf->translation[2] == Approx(3.f));

  const ossia::scene_node* cam_node = nullptr;
  const ossia::scene_node* light_node = nullptr;
  for(const auto& payload : *root.children)
  {
    if(auto* child = ossia::get_if<ossia::scene_node_ptr>(&payload))
    {
      if((*child)->name == "camchild")
        cam_node = child->get();
      if((*child)->name == "lightchild")
        light_node = child->get();
    }
  }
  REQUIRE(cam_node);
  REQUIRE(light_node);

  // Camera / light payloads on the child nodes.
  const ossia::camera_component* cam = nullptr;
  for(const auto& payload : *cam_node->children)
    if(auto* c = ossia::get_if<ossia::camera_component_ptr>(&payload))
      cam = c->get();
  REQUIRE(cam);
  CHECK(cam->yfov == Approx(1.0f));
  CHECK(cam->znear == Approx(0.01f));
  CHECK(cam->zfar == Approx(100.0f));

  const ossia::light_component* light = nullptr;
  for(const auto& payload : *light_node->children)
    if(auto* l = ossia::get_if<ossia::light_component_ptr>(&payload))
      light = l->get();
  REQUIRE(light);
  CHECK(light->color[0] == Approx(1.0f));
  CHECK(light->color[1] == Approx(0.5f));
  CHECK(light->color[2] == Approx(0.25f));
  CHECK(light->intensity == Approx(3.0f));
}

TEST_CASE("glTF: corrupt / truncated inputs fail gracefully",
          "[threedim][gltf]")
{
  TempDir tmp;

  SECTION("empty filename")
  {
    CHECK_FALSE(Threedim::GltfParser::ins::gltf_t::process(
        halp::text_file_view{.bytes = "{}", .filename = {}}));
  }
  SECTION("nonexistent file")
  {
    const std::string p = (tmp.dir / "missing.gltf").string();
    CHECK_FALSE(Threedim::GltfParser::ins::gltf_t::process(
        halp::text_file_view{.bytes = {}, .filename = p}));
  }
  SECTION("truncated JSON")
  {
    const auto bin = triangle_buffer_bytes();
    auto json = triangle_gltf_json(
        "data:application/octet-stream;base64," + b64(bin), true);
    json.resize(json.size() / 2);
    const auto path = tmp.write("trunc.gltf", json);
    const std::string p = path.string();
    CHECK_FALSE(Threedim::GltfParser::ins::gltf_t::process(
        halp::text_file_view{.bytes = {}, .filename = p}));
  }
  SECTION("GLB with bad magic")
  {
    auto glb = make_glb(triangle_gltf_json({}, true), triangle_buffer_bytes());
    glb[0] = 'X';
    const auto path = tmp.write("badmagic.glb", glb);
    const std::string p = path.string();
    CHECK_FALSE(Threedim::GltfParser::ins::gltf_t::process(
        halp::text_file_view{.bytes = {}, .filename = p}));
  }
  SECTION("GLB truncated mid-BIN-chunk")
  {
    auto glb = make_glb(triangle_gltf_json({}, true), triangle_buffer_bytes());
    glb.resize(glb.size() - 40);
    const auto path = tmp.write("trunc.glb", glb);
    const std::string p = path.string();
    CHECK_FALSE(Threedim::GltfParser::ins::gltf_t::process(
        halp::text_file_view{.bytes = {}, .filename = p}));
  }
  SECTION("random binary garbage in a .gltf")
  {
    std::string garbage(256, '\0');
    for(std::size_t i = 0; i < garbage.size(); i++)
      garbage[i] = char((i * 37 + 11) & 0xFF);
    const auto path = tmp.write("garbage.gltf", garbage);
    const std::string p = path.string();
    CHECK_FALSE(Threedim::GltfParser::ins::gltf_t::process(
        halp::text_file_view{.bytes = {}, .filename = p}));
  }
}

// REGRESSION TEST (bug found by this suite 2026-07-16, fixed 2026-07-17):
// a hostile .gltf whose accessor declares `count` far beyond its
// bufferView's byteLength used to drive a heap-buffer-overflow READ in
// fastgltf::iterateAccessor, reached from GltfParser.cpp
// read_float_accessor <- extract_primitive <- emit_node.
// fastgltf::validate() only checks the accessor's *bufferView index*,
// alignment, and count >= 1 — it never checks
//   accessor.byteOffset + count * elementSize <= bufferView.byteLength
// (see 3rdparty/fastgltf/src/fastgltf.cpp, fg::validate).
// GltfParser.cpp now sweeps every accessor's data range against its
// bufferView and the buffer's actually-loaded bytes before iterating
// anything (all_accessors_within_bounds + per-read-site re-checks) and
// rejects the load. This case must load-fail cleanly with no ASAN report.
TEST_CASE("glTF: hostile accessor count beyond bufferView is rejected "
          "cleanly (no OOB read)",
          "[threedim][gltf][gltf-oob-bug]")
{
  TempDir tmp;
  const auto bin = triangle_buffer_bytes();
  auto json = triangle_gltf_json(
      "data:application/octet-stream;base64," + b64(bin), true);
  auto pos = json.find(R"("count":3,"type":"VEC3","min")");
  REQUIRE(pos != std::string::npos);
  json.replace(pos, 9, R"("count":30000)");
  const auto path = tmp.write("oob.gltf", json);
  const std::string p = path.string();
  // The hostile file is rejected outright; previously this OOB-read.
  CHECK_FALSE(Threedim::GltfParser::ins::gltf_t::process(
      halp::text_file_view{.bytes = {}, .filename = p}));
}

// ===========================================================================
// FBX — ufbx bridge
// ===========================================================================

TEST_CASE("FBX: minimal ASCII fbx triangle — ufbx generates missing normals",
          "[threedim][fbx]")
{
  TempDir tmp;
  const auto path = tmp.write("tri.fbx", triangle_ascii_fbx());
  const std::string pathstr = path.string();

  auto apply = Threedim::FbxParser::ins::fbx_t::process(
      halp::text_file_view{.bytes = {}, .filename = pathstr});
  REQUIRE(apply);

  Threedim::FbxParser parser;
  apply(parser);
  REQUIRE(parser.m_raw_state);

  const auto* mesh = find_first_mesh(*parser.m_raw_state);
  REQUIRE(mesh);
  REQUIRE(mesh->primitives.size() >= 1);
  const auto& prim = mesh->primitives[0];

  CHECK(prim.vertex_count == 3);
  CHECK(prim.topology == ossia::primitive_topology::triangles);

  const auto* pos = find_attr(prim, ossia::attribute_semantic::position);
  REQUIRE(pos);

  // Unlike the OBJ path, the FBX path GENERATES missing normals
  // (ufbx opts.generate_missing_normals = true).
  const auto* nrm = find_attr(prim, ossia::attribute_semantic::normal);
  REQUIRE(nrm);
  const float* n = attr_floats(prim, *nrm);
  // Triangle (0,0,0)(1,0,0)(0,1,0) is in the XY plane: normal = +-Z, unit.
  const float len0
      = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  CHECK(len0 == Approx(1.f).margin(1e-4));
  CHECK(std::abs(n[2]) == Approx(1.f).margin(1e-4));
}

TEST_CASE("FBX: corrupt / truncated inputs fail gracefully", "[threedim][fbx]")
{
  TempDir tmp;

  SECTION("empty filename")
  {
    CHECK_FALSE(Threedim::FbxParser::ins::fbx_t::process(
        halp::text_file_view{.bytes = {}, .filename = {}}));
  }
  SECTION("nonexistent file")
  {
    const std::string p = (tmp.dir / "missing.fbx").string();
    CHECK_FALSE(Threedim::FbxParser::ins::fbx_t::process(
        halp::text_file_view{.bytes = {}, .filename = p}));
  }
  SECTION("binary garbage")
  {
    std::string garbage(512, '\0');
    for(std::size_t i = 0; i < garbage.size(); i++)
      garbage[i] = char((i * 131 + 7) & 0xFF);
    const auto path = tmp.write("garbage.fbx", garbage);
    const std::string p = path.string();
    CHECK_FALSE(Threedim::FbxParser::ins::fbx_t::process(
        halp::text_file_view{.bytes = {}, .filename = p}));
  }
  SECTION("truncated binary FBX header")
  {
    // The binary FBX magic, then nothing.
    const std::string trunc = "Kaydara FBX Binary  \x00\x1a\x00";
    const auto path = tmp.write("trunc.fbx", trunc);
    const std::string p = path.string();
    CHECK_FALSE(Threedim::FbxParser::ins::fbx_t::process(
        halp::text_file_view{.bytes = {}, .filename = p}));
  }
  SECTION("valid ASCII structure but zero meshes")
  {
    const std::string empty_doc = R"(; FBX 7.4.0 project file
FBXHeaderExtension:  {
	FBXHeaderVersion: 1003
	FBXVersion: 7400
}
Objects:  {
}
)";
    const auto path = tmp.write("empty.fbx", empty_doc);
    const std::string p = path.string();
    CHECK_FALSE(Threedim::FbxParser::ins::fbx_t::process(
        halp::text_file_view{.bytes = {}, .filename = p}));
  }
}

// ===========================================================================
// AssetLoader — extension dispatch + registry + TRS wrap
// ===========================================================================

TEST_CASE("AssetLoader: dispatches .obj from bytes", "[threedim][assetloader]")
{
  const auto data = read_file(models_dir / "cube.obj");
  if(!data)
    SKIP("ModelsOBJ corpus not available");

  auto apply = Threedim::AssetLoader::ins::asset_t::process(
      halp::text_file_view{
          .bytes = *data,
          .filename = (models_dir / "cube.obj").string()});
  REQUIRE(apply);

  Threedim::AssetLoader loader;
  apply(loader);
  REQUIRE(loader.m_parsed_state);

  const auto* mesh = find_first_mesh(*loader.m_parsed_state);
  REQUIRE(mesh);
  CHECK(mesh->primitives[0].vertex_count == 36);
  // Label = QFileInfo(...).fileName().
  CHECK((*loader.m_parsed_state->roots)[0]->name == "cube.obj");

  // Wrapped state: raw scene re-rooted under a single TRS parent node.
  REQUIRE(loader.m_wrapped_state);
  REQUIRE(loader.m_wrapped_state->roots);
  REQUIRE(loader.m_wrapped_state->roots->size() == 1);
  const auto& wrap_root = *(*loader.m_wrapped_state->roots)[0];
  REQUIRE(wrap_root.children);
  // First payload = the scene_transform carrying the controls' TRS.
  REQUIRE(wrap_root.children->size() == 2);
  const auto* xf
      = ossia::get_if<ossia::scene_transform>(&(*wrap_root.children)[0]);
  REQUIRE(xf);
  CHECK(xf->scale[0] == Approx(1.f));

  // operator() publishes the wrapped state.
  loader();
  CHECK(loader.outputs.scene_out.scene.state == loader.m_wrapped_state);
  CHECK(loader.outputs.scene_out.dirty == ossia::scene_port::dirty_transform);
}

TEST_CASE("AssetLoader: dispatches .gltf / .glb via the file path",
          "[threedim][assetloader]")
{
  TempDir tmp;
  const auto bin = triangle_buffer_bytes();
  const auto json = triangle_gltf_json(
      "data:application/octet-stream;base64," + b64(bin), true);

  SECTION(".gltf")
  {
    const auto path = tmp.write("tri.gltf", json);
    const std::string pathstr = path.string();
    auto apply = Threedim::AssetLoader::ins::asset_t::process(
        halp::text_file_view{.bytes = json, .filename = pathstr});
    REQUIRE(apply);
    Threedim::AssetLoader loader;
    apply(loader);
    REQUIRE(loader.m_parsed_state);
    check_triangle_gltf_scene(*loader.m_parsed_state);
  }
  SECTION(".glb")
  {
    const auto glb = make_glb(triangle_gltf_json({}, true), bin);
    const auto path = tmp.write("tri.glb", glb);
    const std::string pathstr = path.string();
    auto apply = Threedim::AssetLoader::ins::asset_t::process(
        halp::text_file_view{.bytes = glb, .filename = pathstr});
    REQUIRE(apply);
    Threedim::AssetLoader loader;
    apply(loader);
    REQUIRE(loader.m_parsed_state);
    check_triangle_gltf_scene(*loader.m_parsed_state);
  }
  SECTION("extension match is case-insensitive")
  {
    const auto path = tmp.write("TRI.GLTF", json);
    const std::string pathstr = path.string();
    auto apply = Threedim::AssetLoader::ins::asset_t::process(
        halp::text_file_view{.bytes = json, .filename = pathstr});
    REQUIRE(apply);
  }
}

TEST_CASE("AssetLoader: dispatches .stl / .off / .ply geometry formats",
          "[threedim][assetloader]")
{
  TempDir tmp;

  auto load = [](const fs::path& p) {
    const std::string pathstr = p.string();
    auto apply = Threedim::AssetLoader::ins::asset_t::process(
        halp::text_file_view{.bytes = {}, .filename = pathstr});
    REQUIRE(apply);
    auto loader = std::make_shared<Threedim::AssetLoader>();
    apply(*loader);
    REQUIRE(loader->m_parsed_state);
    return loader;
  };

  SECTION(".stl (ASCII, vcglib) — face normal expanded per corner")
  {
    const auto path = tmp.write("tri.stl", R"(solid tri
 facet normal 0 0 1
  outer loop
   vertex 0 0 0
   vertex 1 0 0
   vertex 0 1 0
  endloop
 endfacet
endsolid tri
)");
    auto loader = load(path);
    const auto* mesh = find_first_mesh(*loader->m_parsed_state);
    REQUIRE(mesh);
    const auto& prim = mesh->primitives[0];
    CHECK(prim.vertex_count == 3);
    CHECK(prim.index_type == ossia::index_format::none);
    // Documented behaviour: although STL carries a per-face normal, the
    // vcglib importer does not report IOM_FACENORMAL in its loadmask for
    // this file, so the bridge emits positions only — no normal attribute.
    CHECK(find_attr(prim, ossia::attribute_semantic::normal) == nullptr);
    const auto* pos = find_attr(prim, ossia::attribute_semantic::position);
    REQUIRE(pos);
    const float* p = attr_floats(prim, *pos);
    CHECK(p[3] == Approx(1.f)); // second vertex x
  }
  SECTION(".off (ASCII, vcglib)")
  {
    const auto path = tmp.write("tri.off", "OFF\n3 1 0\n0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n");
    auto loader = load(path);
    const auto* mesh = find_first_mesh(*loader->m_parsed_state);
    REQUIRE(mesh);
    const auto& prim = mesh->primitives[0];
    CHECK(prim.vertex_count == 3);
    const auto* pos = find_attr(prim, ossia::attribute_semantic::position);
    REQUIRE(pos);
    const float* p = attr_floats(prim, *pos);
    CHECK(p[0] == Approx(0.f));
    CHECK(p[3] == Approx(1.f));
  }
  SECTION(".ply (ASCII mesh, miniply)")
  {
    const auto path = tmp.write("tri.ply", R"(ply
format ascii 1.0
element vertex 3
property float x
property float y
property float z
element face 1
property list uchar int vertex_indices
end_header
0 0 0
1 0 0
0 1 0
3 0 1 2
)");
    auto loader = load(path);
    const auto* mesh = find_first_mesh(*loader->m_parsed_state);
    REQUIRE(mesh);
    const auto& prim = mesh->primitives[0];
    CHECK(prim.vertex_count == 3);
    const auto* pos = find_attr(prim, ossia::attribute_semantic::position);
    REQUIRE(pos);
    const float* p = attr_floats(prim, *pos);
    CHECK(p[0] == Approx(0.f));
    CHECK(p[3] == Approx(1.f));
    CHECK(p[7] == Approx(1.f));
  }
  SECTION("corrupt .stl / .off / .ply fail gracefully")
  {
    for(const char* name : {"bad.stl", "bad.off", "bad.ply"})
    {
      const auto path = tmp.write(name, "garbage \x01\x02\x03 not a mesh");
      const std::string pathstr = path.string();
      CHECK_FALSE(Threedim::AssetLoader::ins::asset_t::process(
          halp::text_file_view{.bytes = {}, .filename = pathstr}));
    }
  }
}

TEST_CASE("AssetLoader: unknown / empty inputs yield no apply function",
          "[threedim][assetloader]")
{
  SECTION("empty filename")
  {
    CHECK_FALSE(Threedim::AssetLoader::ins::asset_t::process(
        halp::text_file_view{.bytes = "v 0 0 0", .filename = {}}));
  }
  SECTION("unknown extension")
  {
    CHECK_FALSE(Threedim::AssetLoader::ins::asset_t::process(
        halp::text_file_view{.bytes = "data", .filename = "foo.unknownext"}));
  }
  SECTION("dotless filename")
  {
    CHECK_FALSE(Threedim::AssetLoader::ins::asset_t::process(
        halp::text_file_view{.bytes = "data", .filename = "no_extension"}));
  }
  SECTION("corrupt obj bytes with valid extension")
  {
    CHECK_FALSE(Threedim::AssetLoader::ins::asset_t::process(
        halp::text_file_view{
            .bytes = "v 0 0 0\nf 1 2 9999\n", .filename = "bad.obj"}));
  }
}

// The addon-parser registry: registration, lowercase lookup, last-writer-
// wins, and end-to-end dispatch through AssetLoader for a custom extension.
namespace
{
static std::shared_ptr<const ossia::scene_state> s_canned_state;
static std::shared_ptr<const ossia::scene_state>
canned_parser(const halp::text_file_view&)
{
  return s_canned_state;
}
static std::shared_ptr<const ossia::scene_state>
null_parser(const halp::text_file_view&)
{
  return nullptr;
}
}

TEST_CASE("AssetLoaderRegistry: registration and dispatch",
          "[threedim][assetloader][registry]")
{
  using R = Threedim::AssetLoaderRegistry;

  SECTION("lookup miss returns nullptr")
  {
    CHECK(R::lookup("definitely-not-registered") == nullptr);
    CHECK(R::lookup("") == nullptr);
  }
  SECTION("invalid registrations are ignored")
  {
    R::register_parser("", &canned_parser);
    CHECK(R::lookup("") == nullptr);
    R::register_parser("nullfn", nullptr);
    CHECK(R::lookup("nullfn") == nullptr);
  }
  SECTION("extension keys are lowercased; last writer wins")
  {
    R::register_parser("TSTA", &null_parser);
    CHECK(R::lookup("tsta") == &null_parser);
    R::register_parser("tsta", &canned_parser);
    CHECK(R::lookup("tsta") == &canned_parser);
  }
  SECTION("AssetLoader consults the registry for unknown extensions")
  {
    // Build a canned one-node scene the fake parser returns.
    auto node = std::make_shared<ossia::scene_node>();
    node->name = "from-registry";
    auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
    roots->push_back(node);
    auto st = std::make_shared<ossia::scene_state>();
    st->roots = roots;
    s_canned_state = st;

    R::register_parser("tstb", &canned_parser);

    auto apply = Threedim::AssetLoader::ins::asset_t::process(
        halp::text_file_view{.bytes = "payload", .filename = "thing.TstB"});
    REQUIRE(apply);
    Threedim::AssetLoader loader;
    apply(loader);
    // The parsed state is exactly the parser-returned pointer.
    CHECK(loader.m_parsed_state == st);
    REQUIRE(loader.m_wrapped_state);
    s_canned_state.reset();
  }
  SECTION("registry parser returning null -> no apply function")
  {
    R::register_parser("tstc", &null_parser);
    CHECK_FALSE(Threedim::AssetLoader::ins::asset_t::process(
        halp::text_file_view{.bytes = "x", .filename = "f.tstc"}));
  }
}
