// P2-5 (SPEC-SCENE-RENDER-TESTS.md §3.3): `FBX loads` — the ufbx path yields
// position / normal / UV streams with the expected counts, plus a truncation
// matrix driven through ForkProbe.
//
// WHAT ALREADY EXISTED, and what this file adds. FBX was not virgin ground:
//   * tests/unit/ThreedimLoaderTest.cpp:975-1010 loads one minimal ASCII FBX
//     triangle and checks that ufbx generates the missing normal (|n| == 1,
//     |n.z| == 1, both with margin 1e-4).
//   * tests/unit/AssetLoaderFailure.cpp:635 already runs a byte-prefix
//     truncation matrix on `t.fbx` — but on that same positions-only ASCII
//     fixture, so no prefix ever cuts inside a normal or UV array.
//   * tests/threedim/GeometryLoaderFormats.cpp:424 only checks that OBJ bytes
//     named `.fbx` are refused.
// Nothing anywhere loads a BINARY FBX, nobody asserts a UV stream, a
// triangulated polygon count, a derived tangent, or an attribute-by-attribute
// layout. That is what this file is for. Concretely, new here:
//   (a) an ASCII FBX 7500 scene with three meshes — triangle with normals+UVs,
//       quad (triangulation), positions-only (normal generation) — with exact
//       per-attribute counts, values and bounds;
//   (b) the first binary-FBX coverage in the tree: a byte-for-byte synthesized
//       FBX 7400 *binary* container, asserted to decode to the same streams as
//       the equivalent ASCII document;
//   (c) a ForkProbe truncation matrix over that binary container (every prefix
//       rejected — the binary framing makes that exact) and over the full ASCII
//       scene (prefixes that ARE accepted must still publish walkable buffers);
//   (d) an optional real-asset leg, SKIPped with an exact command when absent.
//
// Entry point under test — the exact static AssetLoader calls
// (AssetLoader.cpp:151-153 dispatches `.fbx` here):
//
//   Threedim::FbxParser::ins::fbx_t::process(file_type tv)
//     FbxParser.cpp:1021-1022  empty filename -> {}
//     FbxParser.cpp:1025       opts.generate_missing_normals = true
//     FbxParser.cpp:1026       opts.normalize_normals = true
//     FbxParser.cpp:1030-1033  target axes +X/+Y/+Z, target_unit_meters = 1.0
//     FbxParser.cpp:1040       space_conversion = ADJUST_TRANSFORMS
//     FbxParser.cpp:1049       ufbx_load_file(...); null scene -> {}
//     FbxParser.cpp:1068-1069  zero extracted nodes -> {}
//     FbxParser.cpp:438        vertex_count = part.num_triangles * 3
//                              (the FBX path is UNINDEXED — see below)
//     FbxParser.cpp:442-445    has_normals / has_uv / has_colors / has_tangents
//     FbxParser.cpp:447-457    positions + local AABB
//     FbxParser.cpp:459-478    normals, re-normalized
//     FbxParser.cpp:480-488    texcoords, taken through verbatim (no V flip)
//     FbxParser.cpp:538-547    no TANGENT channel but normals+UVs present ->
//                              mikktspace synthesises tangents
//     FbxParser.cpp:846-849    topology = triangles, index_type = none,
//                              index_count = 0  (no index buffer at all)
//     FbxParser.cpp:875-884    attribute order position, normal, texcoord0,
//                              color0, tangent — buffer_index assigned in that
//                              order, skipping absent streams
//     FbxParser.cpp:802-808    the ufbx synthetic root is skipped; its children
//                              become scene_state::roots
//     FbxParser.cpp:955-972    per node: the local transform payload first,
//                              then the mesh_component
//
// Registration (tests/unit/CMakeLists.txt) — place immediately after the
// test_unit_threedim_gltf_loader block (tests/unit/CMakeLists.txt:853-862),
// INSIDE the same `if(TARGET score_plugin_threedim)` guard, before its
// `endif()`. Same wiring as its neighbours:
//
//   # P2-5: FBX loads. First binary-FBX coverage in the tree: synthesized
//   # ASCII 7500 + binary 7400 fixtures, exact position/normal/UV counts and
//   # values, mikktspace tangent closed form, and ForkProbe truncation
//   # matrices over both containers. Same engine sources / libs as
//   # test_unit_threedim_gltf_loader above.
//   score_add_test(test_unit_threedim_fbx_loader
//     SOURCES
//       FbxLoaderTest.cpp
//       ${_threedim_hidden}
//     PLUGINS score_plugin_threedim score_plugin_gfx score_plugin_avnd
//     LIBS test_unit_threedim_3rdparty fastgltf spz "${QT_PREFIX}::Gui")
//   target_include_directories(test_unit_threedim_fbx_loader SYSTEM PRIVATE
//     "${SCORE_ROOT_SOURCE_DIR}/3rdparty/vcglib"
//     "${SCORE_ROOT_SOURCE_DIR}/3rdparty/eigen")
//
// ctest target: test_unit_threedim_fbx_loader.
//
// That block is not optional: cmake/ScoreTestRegistrationGuard.cmake
// FATAL_ERRORs the configure on any .cpp under tests/ that no ctest entry
// reaches, so this file breaks `cmake` until the block above is in
// tests/unit/CMakeLists.txt.
//
// Status: run green as written — 7214 assertions, 3 cases passed and the
// real-asset case SKIPped, then 238 more assertions when the cube named below
// is placed in the corpus directory. Both negative controls below were
// applied and measured, not predicted.
//
// NEGATIVE CONTROL (spec: "drop the normal accessor"). The hook is
// FbxParser.cpp:459 — `if(has_normals)`, guarding the normal extraction at
// FbxParser.cpp:459-478. Exact edit: change line 459 to `if(false)`.
//
// Dropping it takes the tangent stream with it: FbxParser.cpp:538 still enters
// the mikktspace branch (its guard reads the ufbx flags, not sp.normals), but
// TangentUtils.hpp:34-35 returns {} on a null normals pointer. So Tri and Quad
// lose 2 of their 4 attributes and Bare loses 1 of 2.
//
// Measured, with the edit applied and the file otherwise untouched: 5 failed
// assertions in 3 of the 4 test cases, 7039 still passing. The failures are
// the attribute-count REQUIREs that guard each stream comparison —
//   FbxLoaderTest "TriMesh"  attributes.size() == 4
//   FbxLoaderTest "QuadMesh" attributes.size() == 4
//   FbxLoaderTest "BareMesh" attributes.size() == 2
//   the ASCII/binary parity case, attributes.size() == 2
//   the real-asset cube's REQUIRE(nrm)
// — which abort their sections, so the per-value normal and tangent checks
// below them do not get to fail individually. Everything downstream of a
// normal is covered by those five.
//
// It leaves green: every vertex/triangle count, every position value and AABB,
// every UV value and the uv == pos/2 relation, index_type/index_count/topology,
// the root names and transforms, and BOTH truncation matrices (verified: the
// truncation test case is the one of the four that still passes).
//
// A second, narrower control aimed at normal GENERATION rather than normal
// extraction: FbxParser.cpp:1025 `opts.generate_missing_normals = true` ->
// `false`. Measured: 2 failed assertions in 2 test cases — the "BareMesh"
// section and the ASCII/binary parity case, the two whose fixtures declare no
// LayerElementNormal. Tri, Quad and the real-asset cube carry normals in the
// file and stay green, which is what separates the two controls.
//
// FIXTURES. Everything the assertions depend on is synthesized by this test —
// no download, no committed binary. The ASCII document is written out verbatim
// below and every number in it is visible in the source; the binary container
// is built record by record from the same numbers. The real-asset leg is
// strictly optional and SKIPs with the exact command to satisfy it. Spec §3.4
// item 4 asks for a small `.fbx` in fetch-real-assets.sh; that script is not
// mine to edit, so the SKIP message names both the one-line copy that needs no
// network and the `fetch` line that would belong in the script.
//
// Unverified: this file assumes a little-endian host when writing the binary
// FBX container (as do the GLB writers in tests/unit/GltfLoaderTest.cpp and
// tests/unit/AssetLoaderFailure.cpp). Not exercised on a big-endian target.

#include <Threedim/FbxParser.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <score_test/ForkProbe.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
using Catch::Approx;

namespace
{
// ---------------------------------------------------------------------------
// Temp-file plumbing — same shape as tests/unit/GltfLoaderTest.cpp:
// one tag per process so two concurrent copies of this executable cannot
// collide on a fixture path.
// ---------------------------------------------------------------------------
const std::string& uniqueTag()
{
  static const std::string tag = std::to_string(std::random_device{}());
  return tag;
}

struct TempDir
{
  fs::path dir;
  TempDir()
  {
    dir = fs::temp_directory_path() / ("score-threedim-fbx-" + uniqueTag());
    fs::create_directories(dir);
  }
  ~TempDir()
  {
    std::error_code ec;
    fs::remove_all(dir, ec);
  }
  std::string write(const std::string& name, std::string_view bytes) const
  {
    const auto p = dir / name;
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(bytes.data(), std::streamsize(bytes.size()));
    f.close();
    return p.string();
  }
};

template <typename T>
void put(std::string& s, T v)
{
  char buf[sizeof(T)];
  std::memcpy(buf, &v, sizeof(T));
  s.append(buf, sizeof(T));
}

std::optional<std::string> read_file(const fs::path& p)
{
  std::ifstream f(p, std::ios::binary);
  if(!f)
    return std::nullopt;
  return std::string(
      (std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// The default destination of tests/integration/threedim-render/
// fetch-real-assets.sh ("${1:-$HOME/ossia/threedim-assets}") — the same
// corpus directory tests/unit/GltfLoaderTest.cpp reads.
fs::path assets_dir()
{
  const char* home = ::getenv("HOME");
  return fs::path{home ? home : "."} / "ossia" / "threedim-assets";
}

// ---------------------------------------------------------------------------
// Driving idiom of tests/unit/ThreedimLoaderTest.cpp:975-985 — process()
// returns an apply-lambda (empty on rejection); applying it to a fresh parser
// stores the scene in m_raw_state.
//
// Note on FbxParser.cpp:1049: it hands `tv.filename.data()` straight to the C
// API `ufbx_load_file`, which needs NUL termination — unlike GltfParser.cpp,
// which copies the view into a std::filesystem::path first. Every caller here
// (and AssetLoader's) backs the view with a std::string, so the byte after the
// view is the string's NUL. Flagged, not exercised: a caller passing a
// substring view would read past the end.
// ---------------------------------------------------------------------------
std::unique_ptr<Threedim::FbxParser> load_fbx(const std::string& filename)
{
  halp::text_file_view tv;
  tv.filename = filename; // FbxParser reads from disk; bytes stay empty
  tv.bytes = std::string_view{};
  auto apply = Threedim::FbxParser::ins::fbx_t::process(tv);
  if(!apply)
    return nullptr;
  auto parser = std::make_unique<Threedim::FbxParser>();
  apply(*parser);
  return parser;
}

// ---------------------------------------------------------------------------
// scene_state inspection helpers (pattern: tests/unit/GltfLoaderTest.cpp)
// ---------------------------------------------------------------------------
const ossia::mesh_component* mesh_of(const ossia::scene_node& n)
{
  if(!n.children)
    return nullptr;
  for(const auto& payload : *n.children)
    if(auto* mc = ossia::get_if<ossia::mesh_component_ptr>(&payload))
      return mc->get();
  return nullptr;
}

const ossia::scene_transform* transform_of(const ossia::scene_node& n)
{
  if(!n.children)
    return nullptr;
  for(const auto& payload : *n.children)
    if(auto* tr = ossia::get_if<ossia::scene_transform>(&payload))
      return tr;
  return nullptr;
}

// FbxParser emits one root scene_node per ufbx root child (FbxParser.cpp:
// 802-808 / 990-993). Look them up by name so no assertion depends on the
// order ufbx happens to walk the Connections block in.
const ossia::scene_node*
root_named(const ossia::scene_state& s, std::string_view name)
{
  if(!s.roots)
    return nullptr;
  for(const auto& r : *s.roots)
    if(r && r->name == name)
      return r.get();
  return nullptr;
}

const ossia::vertex_attribute*
find_attr(const ossia::mesh_primitive& p, ossia::attribute_semantic sem)
{
  for(const auto& a : p.attributes)
    if(a.semantic == sem)
      return &a;
  return nullptr;
}

const ossia::buffer_data*
buffer_of(const ossia::mesh_primitive& p, const ossia::vertex_attribute& a)
{
  if(a.buffer_index >= p.vertex_buffers.size())
    return nullptr;
  const auto& br = p.vertex_buffers[a.buffer_index];
  if(!br)
    return nullptr;
  return ossia::get_if<ossia::buffer_data>(&br->resource);
}

const float*
attr_floats(const ossia::mesh_primitive& p, const ossia::vertex_attribute& a)
{
  const auto* bd = buffer_of(p, a);
  REQUIRE(bd);
  REQUIRE(bd->data);
  return reinterpret_cast<const float*>(
      reinterpret_cast<const char*>(bd->data.get()) + a.byte_offset);
}

// Bytes one vertex occupies in its own buffer. FbxParser gives every stream a
// dedicated, tightly-packed buffer (FbxParser.cpp:859-873), so this is also
// the buffer's byte_size divided by vertex_count.
std::size_t bytes_per_vertex(ossia::vertex_format f)
{
  switch(f)
  {
    case ossia::vertex_format::float2:
      return 8;
    case ossia::vertex_format::float3:
      return 12;
    case ossia::vertex_format::float4:
      return 16;
    case ossia::vertex_format::uint16x4:
      return 8;
    default:
      return 0;
  }
}

// ---------------------------------------------------------------------------
// Invariants every primitive the FBX path emits must satisfy, all read off
// FbxParser.cpp rather than off any particular fixture. Applied to the
// synthesized fixtures AND to whatever real asset the corpus leg finds.
// ---------------------------------------------------------------------------
void check_primitive_invariants(const ossia::mesh_primitive& prim)
{
  // FbxParser.cpp:846-852: triangle list, never indexed. The extraction at
  // FbxParser.cpp:409/438 emits three unshared vertices per triangle, so the
  // vertex count is always a multiple of 3 and there is no index buffer at all
  // — the structural difference from the glTF path, which does index.
  CHECK(prim.topology == ossia::primitive_topology::triangles);
  CHECK(prim.index_type == ossia::index_format::none);
  CHECK(prim.index_count == 0u);
  CHECK_FALSE(prim.index_buffer);
  REQUIRE(prim.vertex_count > 0u);
  CHECK(prim.vertex_count % 3u == 0u);

  // FbxParser.cpp:859-873: one buffer per present attribute, appended in
  // lockstep, byte_offset always 0, buffer_index the running counter.
  REQUIRE(prim.attributes.size() == prim.vertex_buffers.size());
  for(std::size_t i = 0; i < prim.attributes.size(); i++)
  {
    INFO("attribute " << i);
    const auto& a = prim.attributes[i];
    CHECK(a.buffer_index == uint32_t(i));
    CHECK(a.byte_offset == 0u);
    CHECK(a.rate == ossia::vertex_attribute::input_rate::per_vertex);
    const std::size_t bpv = bytes_per_vertex(a.format);
    REQUIRE(bpv > 0);
    CHECK(a.byte_stride == uint32_t(bpv));
    const auto* bd = buffer_of(prim, a);
    REQUIRE(bd);
    REQUIRE(bd->data);
    // The buffer must actually hold every vertex it claims: this is the
    // "never publishes a half-stream" half of the contract.
    CHECK(bd->byte_size == int64_t(prim.vertex_count * bpv));
    CHECK(bd->usage_hint == ossia::buffer_data::usage::vertex_buffer);
  }

  // Position is unconditional (FbxParser.cpp:447-452, ScenePart comment in
  // FbxParser.hpp: "always present").
  const auto* pos = find_attr(prim, ossia::attribute_semantic::position);
  REQUIRE(pos);
  CHECK(pos->format == ossia::vertex_format::float3);

  const auto* nrm = find_attr(prim, ossia::attribute_semantic::normal);
  const auto* uv = find_attr(prim, ossia::attribute_semantic::texcoord0);
  const auto* tan = find_attr(prim, ossia::attribute_semantic::tangent);

  if(nrm)
  {
    // FbxParser.cpp:465-476 divides by the length, so every emitted normal is
    // unit — or the (0,1,0) fallback, which is also unit.
    CHECK(nrm->format == ossia::vertex_format::float3);
    const float* n = attr_floats(prim, *nrm);
    for(uint32_t v = 0; v < prim.vertex_count; v++)
    {
      INFO("normal " << v);
      const float len = std::sqrt(
          n[v * 3] * n[v * 3] + n[v * 3 + 1] * n[v * 3 + 1]
          + n[v * 3 + 2] * n[v * 3 + 2]);
      CHECK(len == Approx(1.f).margin(1e-5));
    }
  }
  if(uv)
    CHECK(uv->format == ossia::vertex_format::float2);

  // FbxParser.cpp:538-547: normals + UVs and no TANGENT channel in the file
  // means mikktspace runs, so a primitive that has both must have tangents.
  if(nrm && uv)
    CHECK(tan != nullptr);
  if(tan)
  {
    CHECK(tan->format == ossia::vertex_format::float4);
    const float* t = attr_floats(prim, *tan);
    for(uint32_t v = 0; v < prim.vertex_count; v++)
    {
      INFO("tangent " << v);
      const float len = std::sqrt(
          t[v * 4] * t[v * 4] + t[v * 4 + 1] * t[v * 4 + 1]
          + t[v * 4 + 2] * t[v * 4 + 2]);
      CHECK(len == Approx(1.f).margin(1e-5));
      // mikktspace writes the handedness as literally 1.0f or -1.0f
      // (m_setTSpaceBasic, wired at TangentUtils.hpp:114-125), so this is an
      // exact comparison, not a tolerance.
      CHECK((t[v * 4 + 3] == 1.0f || t[v * 4 + 3] == -1.0f));
    }
  }
}

// ===========================================================================
// Fixture 1 — an ASCII FBX 7500 scene, three meshes.
//
// Written out in full so every expected number below is visible in the same
// file as the assertion that uses it. Choices that matter:
//
//  * UnitScaleFactor 100. FBX measures in centimetres at UnitScaleFactor 1, so
//    ufbx would report scene unit_meters = 0.01 and, against
//    opts.target_unit_meters = 1.0 (FbxParser.cpp:1033) with
//    space_conversion = ADJUST_TRANSFORMS (FbxParser.cpp:1040), fold a 0.01
//    factor into every root node's scale. UnitScaleFactor 100 makes the file's
//    unit exactly one metre, so the conversion factor is exactly 1 and the
//    node transforms come out identity — which the test asserts, pinning that
//    conversion rather than ignoring it.
//  * UpAxis / FrontAxis / CoordAxis are written to +Y / +Z / +X, matching
//    opts.target_axes at FbxParser.cpp:1030-1032, so no axis conversion is
//    applied either and mesh-local positions are exactly the file's numbers.
//  * every coordinate is a dyadic rational (0.25, 1.5, 3.75, 0.125, 8.5, ...),
//    exactly representable in binary64 AND binary32, so double -> float at
//    FbxParser.cpp:451 is lossless and the comparisons below are `==`.
//
// The three meshes:
//   TriMesh  — 1 triangular face, normals + UVs given ByPolygonVertex/Direct.
//              1 face -> 1 triangle -> 3 vertices.
//   QuadMesh — 1 quad face, normals + UVs. A convex n-gon triangulates to
//              n - 2 triangles, so 4 - 2 = 2 triangles -> 6 vertices.
//   BareMesh — 1 triangular face, positions only: no LayerElementNormal at
//              all, so the normal stream can only come from
//              opts.generate_missing_normals (FbxParser.cpp:1025).
// ===========================================================================
const char* const ascii_scene_fbx = R"(; FBX 7.5.0 project file
FBXHeaderExtension:  {
	FBXHeaderVersion: 1003
	FBXVersion: 7500
	Creator: "score test fixture"
}
GlobalSettings:  {
	Version: 1000
	Properties70:  {
		P: "UpAxis", "int", "Integer", "",1
		P: "UpAxisSign", "int", "Integer", "",1
		P: "FrontAxis", "int", "Integer", "",2
		P: "FrontAxisSign", "int", "Integer", "",1
		P: "CoordAxis", "int", "Integer", "",0
		P: "CoordAxisSign", "int", "Integer", "",1
		P: "UnitScaleFactor", "double", "Number", "",100
	}
}
Objects:  {
	Geometry: 1000, "Geometry::TriMesh", "Mesh" {
		Vertices: *9 {
			a: 0.25,-1.5,3.75,1,2,-0.5,-2.25,0.125,8.5
		}
		PolygonVertexIndex: *3 {
			a: 0,1,-3
		}
		GeometryVersion: 124
		LayerElementNormal: 0 {
			Version: 101
			Name: ""
			MappingInformationType: "ByPolygonVertex"
			ReferenceInformationType: "Direct"
			Normals: *9 {
				a: 0,0,1,0,0,1,0,0,1
			}
		}
		LayerElementUV: 0 {
			Version: 101
			Name: "UVMap"
			MappingInformationType: "ByPolygonVertex"
			ReferenceInformationType: "Direct"
			UV: *6 {
				a: 0,0,1,0,0,1
			}
		}
		Layer: 0 {
			Version: 100
			LayerElement:  {
				Type: "LayerElementNormal"
				TypedIndex: 0
			}
			LayerElement:  {
				Type: "LayerElementUV"
				TypedIndex: 0
			}
		}
	}
	Model: 2000, "Model::Tri", "Mesh" {
		Version: 232
		Properties70:  {
			P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0
		}
	}
	Geometry: 1001, "Geometry::QuadMesh", "Mesh" {
		Vertices: *12 {
			a: 0,0,0,2,0,0,2,2,0,0,2,0
		}
		PolygonVertexIndex: *4 {
			a: 0,1,2,-4
		}
		GeometryVersion: 124
		LayerElementNormal: 0 {
			Version: 101
			Name: ""
			MappingInformationType: "ByPolygonVertex"
			ReferenceInformationType: "Direct"
			Normals: *12 {
				a: 0,0,1,0,0,1,0,0,1,0,0,1
			}
		}
		LayerElementUV: 0 {
			Version: 101
			Name: "UVMap"
			MappingInformationType: "ByPolygonVertex"
			ReferenceInformationType: "Direct"
			UV: *8 {
				a: 0,0,1,0,1,1,0,1
			}
		}
		Layer: 0 {
			Version: 100
			LayerElement:  {
				Type: "LayerElementNormal"
				TypedIndex: 0
			}
			LayerElement:  {
				Type: "LayerElementUV"
				TypedIndex: 0
			}
		}
	}
	Model: 2001, "Model::Quad", "Mesh" {
		Version: 232
		Properties70:  {
			P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0
		}
	}
	Geometry: 1002, "Geometry::BareMesh", "Mesh" {
		Vertices: *9 {
			a: 0,0,0,1,0,0,0,1,0
		}
		PolygonVertexIndex: *3 {
			a: 0,1,-3
		}
		GeometryVersion: 124
	}
	Model: 2002, "Model::Bare", "Mesh" {
		Version: 232
	}
}
Connections:  {
	C: "OO",2000,0
	C: "OO",1000,2000
	C: "OO",2001,0
	C: "OO",1001,2001
	C: "OO",2002,0
	C: "OO",1002,2002
}
)";

// The same single triangle as BareMesh, alone in its own ASCII document —
// used as the ASCII half of the container-parity case.
const char* const ascii_tri_fbx = R"(; FBX 7.5.0 project file
FBXHeaderExtension:  {
	FBXVersion: 7500
}
GlobalSettings:  {
	Version: 1000
	Properties70:  {
		P: "UnitScaleFactor", "double", "Number", "",100
	}
}
Objects:  {
	Geometry: 1000, "Geometry::Tri", "Mesh" {
		Vertices: *9 {
			a: 0,0,0,1,0,0,0,1,0
		}
		PolygonVertexIndex: *3 {
			a: 0,1,-3
		}
		GeometryVersion: 124
	}
	Model: 2000, "Model::Tri", "Mesh" {
		Version: 232
	}
}
Connections:  {
	C: "OO",2000,0
	C: "OO",1000,2000
}
)";

// ===========================================================================
// Fixture 2 — the SAME triangle as a BINARY FBX 7400 container, built record
// by record.
//
// Layout, per the (community-documented, ufbx-implemented) binary FBX format:
//
//   offset  0  21 bytes "Kaydara FBX Binary  \0"
//   offset 21  0x1A 0x00
//   offset 23  uint32 version = 7400
//   offset 27  a sequence of records, then a 13-byte all-zero NULL record
//              terminating the top level.
//
//   A record, for version < 7500 (32-bit offsets; 7500 widens these three
//   fields to uint64 and the NULL record to 25 bytes):
//     uint32 EndOffset       -- absolute offset of the byte AFTER this record
//     uint32 NumProperties
//     uint32 PropertyListLen -- bytes
//     uint8  NameLen; char Name[NameLen]
//     properties...
//     nested records..., then a 13-byte NULL record IF there are any
//
//   Properties used here: 'I' int32, 'L' int64, 'D' double, 'S' uint32 len +
//   bytes, 'd'/'i' arrays as uint32 count + uint32 encoding(0 = raw) + uint32
//   byte length + raw payload.
//
//   Object names in the binary form are "<name>\0\1<class>", which is what the
//   ASCII "Class::Name" spelling encodes.
//
// Two things follow from EndOffset being ABSOLUTE and the top level being
// NULL-terminated, and they are what makes the truncation matrix exact: every
// record's EndOffset points at a byte the file must still contain, and the
// parse only completes when the terminating NULL record is reached at the end.
// A strict byte prefix therefore always breaks one or the other. That is a
// property of the container, derived here, not a number read off a run.
// ===========================================================================
struct FbxRecord
{
  std::string name;
  std::vector<std::string> props;
  std::vector<FbxRecord> children;
};

std::string prop_i32(int32_t v)
{
  std::string s = "I";
  put(s, v);
  return s;
}
std::string prop_i64(int64_t v)
{
  std::string s = "L";
  put(s, v);
  return s;
}
std::string prop_f64(double v)
{
  std::string s = "D";
  put(s, v);
  return s;
}
std::string prop_str(std::string_view v)
{
  std::string s = "S";
  put<uint32_t>(s, uint32_t(v.size()));
  s.append(v);
  return s;
}
std::string prop_f64_array(const std::vector<double>& v)
{
  std::string payload;
  for(double d : v)
    put(payload, d);
  std::string s = "d";
  put<uint32_t>(s, uint32_t(v.size()));
  put<uint32_t>(s, 0); // encoding 0 = uncompressed
  put<uint32_t>(s, uint32_t(payload.size()));
  s += payload;
  return s;
}
std::string prop_i32_array(const std::vector<int32_t>& v)
{
  std::string payload;
  for(int32_t d : v)
    put(payload, d);
  std::string s = "i";
  put<uint32_t>(s, uint32_t(v.size()));
  put<uint32_t>(s, 0);
  put<uint32_t>(s, uint32_t(payload.size()));
  s += payload;
  return s;
}

// `start` is this record's own absolute offset — EndOffset is absolute, so the
// serializer has to thread it down the tree.
std::string serialize_record(const FbxRecord& n, uint32_t start)
{
  std::string proplist;
  for(const auto& p : n.props)
    proplist += p;

  const uint32_t header = 4 + 4 + 4 + 1 + uint32_t(n.name.size());
  uint32_t off = start + header + uint32_t(proplist.size());

  std::string body;
  for(const auto& c : n.children)
  {
    auto s = serialize_record(c, off);
    body += s;
    off += uint32_t(s.size());
  }
  if(!n.children.empty())
  {
    body.append(13, '\0'); // NULL record closing the child list
    off += 13;
  }

  std::string out;
  put<uint32_t>(out, off); // EndOffset
  put<uint32_t>(out, uint32_t(n.props.size()));
  put<uint32_t>(out, uint32_t(proplist.size()));
  out.push_back(char(uint8_t(n.name.size())));
  out += n.name;
  out += proplist;
  out += body;
  return out;
}

std::string binary_tri_fbx()
{
  const std::string geom_name = std::string("Tri") + '\0' + '\1' + "Geometry";
  const std::string model_name = std::string("Tri") + '\0' + '\1' + "Model";

  const std::vector<FbxRecord> roots = {
      {"FBXHeaderExtension", {}, {{"FBXVersion", {prop_i32(7400)}, {}}}},
      {"GlobalSettings",
       {},
       {{"Version", {prop_i32(1000)}, {}},
        {"Properties70",
         {},
         {{"P",
           {prop_str("UnitScaleFactor"), prop_str("double"),
            prop_str("Number"), prop_str(""), prop_f64(100.0)},
           {}}}}}},
      {"Objects",
       {},
       {{"Geometry",
         {prop_i64(1000), prop_str(geom_name), prop_str("Mesh")},
         {{"Vertices", {prop_f64_array({0, 0, 0, 1, 0, 0, 0, 1, 0})}, {}},
          // FBX marks the last index of a polygon by bitwise NOT: ~2 == -3.
          {"PolygonVertexIndex", {prop_i32_array({0, 1, -3})}, {}},
          {"GeometryVersion", {prop_i32(124)}, {}}}},
        {"Model",
         {prop_i64(2000), prop_str(model_name), prop_str("Mesh")},
         {{"Version", {prop_i32(232)}, {}}}}}},
      {"Connections",
       {},
       {// model -> scene root, geometry -> model
        {"C", {prop_str("OO"), prop_i64(2000), prop_i64(0)}, {}},
        {"C", {prop_str("OO"), prop_i64(1000), prop_i64(2000)}, {}}}},
  };

  std::string out("Kaydara FBX Binary  ", 20);
  out.push_back('\0');
  out.push_back('\x1a');
  out.push_back('\0');
  put<uint32_t>(out, 7400);

  uint32_t off = uint32_t(out.size());
  for(const auto& r : roots)
  {
    auto s = serialize_record(r, off);
    out += s;
    off += uint32_t(s.size());
  }
  out.append(13, '\0'); // top-level terminator; nothing follows it
  return out;
}

} // namespace

// ===========================================================================
// (a) the ASCII scene: three meshes, exact counts and values
// ===========================================================================

TEST_CASE(
    "FBX ASCII: position / normal / UV streams with the derived counts",
    "[threedim][fbx][loader]")
{
  TempDir tmp;
  const auto path = tmp.write("scene.fbx", ascii_scene_fbx);

  auto parser = load_fbx(path);
  REQUIRE(parser);
  REQUIRE(parser->m_raw_state);
  const auto& state = *parser->m_raw_state;

  // Three Models are connected to the scene root (id 0) in Connections, so
  // ufbx's synthetic root has three children, and FbxParser.cpp:802-808 turns
  // each into a root scene_node.
  REQUIRE(state.roots);
  CHECK(state.roots->size() == 3u);
  // No material is declared or connected, so register_material is only ever
  // called with nullptr (FbxParser.cpp:644-649 -> :127-128) and the published
  // list is empty rather than absent (FbxParser.cpp:997-1000).
  REQUIRE(state.materials);
  CHECK(state.materials->empty());
  // No skin deformer anywhere -> no skeleton published (FbxParser.cpp:
  // 1005-1010).
  CHECK_FALSE(state.skeletons);
  CHECK(state.version == 1);
  CHECK(state.dirty_index == 1);

  SECTION("TriMesh: one triangular face -> 3 vertices, all four streams")
  {
    const auto* node = root_named(state, "Tri");
    REQUIRE(node);

    // FbxParser.cpp:955 pushes the local transform before the mesh. The file
    // gives Lcl Translation 0,0,0 and no rotation or scaling; UnitScaleFactor
    // 100 makes the unit conversion factor exactly 1 (see the fixture note),
    // and the target axes match the file's, so the TRS is exactly identity.
    // This is the assertion that pins FbxParser.cpp:1030-1033 + :1040.
    const auto* trs = transform_of(*node);
    REQUIRE(trs);
    CHECK(trs->translation[0] == 0.0f);
    CHECK(trs->translation[1] == 0.0f);
    CHECK(trs->translation[2] == 0.0f);
    CHECK(trs->rotation[0] == 0.0f);
    CHECK(trs->rotation[1] == 0.0f);
    CHECK(trs->rotation[2] == 0.0f);
    CHECK(trs->rotation[3] == 1.0f);
    CHECK(trs->scale[0] == 1.0f);
    CHECK(trs->scale[1] == 1.0f);
    CHECK(trs->scale[2] == 1.0f);

    const auto* mesh = mesh_of(*node);
    REQUIRE(mesh);
    // One ufbx material_part (no materials -> one catch-all part), so one
    // primitive (FbxParser.cpp:769-776).
    REQUIRE(mesh->primitives.size() == 1u);
    const auto& prim = mesh->primitives[0];
    check_primitive_invariants(prim);

    // 1 face, 3-gon -> 3 - 2 = 1 triangle -> 1 * 3 = 3 vertices
    // (FbxParser.cpp:438).
    CHECK(prim.vertex_count == 3u);

    // position + normal + texcoord0 + tangent. Tangent is not in the file;
    // it is synthesized because normals and UVs are both present
    // (FbxParser.cpp:538-547). No colours are declared, so color0 is absent
    // and the tangent lands at buffer_index 3, not 4 (FbxParser.cpp:875-884).
    REQUIRE(prim.attributes.size() == 4u);
    CHECK(prim.vertex_buffers.size() == 4u);
    CHECK(prim.attributes[0].semantic == ossia::attribute_semantic::position);
    CHECK(prim.attributes[1].semantic == ossia::attribute_semantic::normal);
    CHECK(prim.attributes[2].semantic == ossia::attribute_semantic::texcoord0);
    CHECK(prim.attributes[3].semantic == ossia::attribute_semantic::tangent);
    CHECK_FALSE(find_attr(prim, ossia::attribute_semantic::color0));
    CHECK_FALSE(find_attr(prim, ossia::attribute_semantic::joints0));
    CHECK_FALSE(find_attr(prim, ossia::attribute_semantic::weights0));

    // Triangulating a 3-gon is the identity, so the emitted order is the
    // polygon's own order 0,1,2 — i.e. the Vertices array verbatim.
    const float expected_pos[9]
        = {0.25f, -1.5f, 3.75f, 1.f, 2.f, -0.5f, -2.25f, 0.125f, 8.5f};
    const auto* apos = find_attr(prim, ossia::attribute_semantic::position);
    REQUIRE(apos);
    const float* p = attr_floats(prim, *apos);
    for(int i = 0; i < 9; i++)
    {
      INFO("position float " << i);
      CHECK(p[i] == expected_pos[i]); // exact: dyadic rationals, no scaling
    }

    // AABB over exactly those nine floats (FbxParser.cpp:455-457 ->
    // ossia::compute_aabb_from_positions, geometry_port.hpp:614-628).
    CHECK(prim.bounds.min[0] == -2.25f);
    CHECK(prim.bounds.min[1] == -1.5f);
    CHECK(prim.bounds.min[2] == -0.5f);
    CHECK(prim.bounds.max[0] == 1.0f);
    CHECK(prim.bounds.max[1] == 2.0f);
    CHECK(prim.bounds.max[2] == 8.5f);

    // Normals: the file says (0,0,1) for all three; length is exactly 1 so the
    // re-normalization at FbxParser.cpp:465-471 multiplies by exactly 1.
    // NEGATIVE CONTROL: FbxParser.cpp:459 -> `if(false)` reddens from here.
    const auto* nrm = find_attr(prim, ossia::attribute_semantic::normal);
    REQUIRE(nrm);
    const float* n = attr_floats(prim, *nrm);
    for(int v = 0; v < 3; v++)
    {
      INFO("normal " << v);
      CHECK(n[v * 3 + 0] == 0.0f);
      CHECK(n[v * 3 + 1] == 0.0f);
      CHECK(n[v * 3 + 2] == 1.0f);
    }

    // UVs: the file's UV array verbatim, no V flip (FbxParser.cpp:484-486
    // copies uv.x / uv.y straight through).
    const auto* uv = find_attr(prim, ossia::attribute_semantic::texcoord0);
    REQUIRE(uv);
    const float* t = attr_floats(prim, *uv);
    const float expected_uv[6] = {0.f, 0.f, 1.f, 0.f, 0.f, 1.f};
    for(int i = 0; i < 6; i++)
    {
      INFO("uv float " << i);
      CHECK(t[i] == expected_uv[i]);
    }

    // Tangent, closed form. With uv0 = (0,0), uv1 = (1,0), uv2 = (0,1) the UV
    // Jacobian is the identity, so the raw dU direction is e1 = p1 - p0 =
    // (0.75, 3.5, -4.25). mikktspace returns it Gram-Schmidt'd against the
    // normal and normalized, and N = (0,0,1) here, so the expected tangent is
    // normalize(0.75, 3.5, 0) = (0.75, 3.5, 0) / sqrt(12.8125).
    // Handedness: cross(N, T) = (-0.9778, 0.2095, 0) has a positive dot with
    // the dV direction e2 - (e2.N)N = (-2.5, 1.625, 0), so w = +1.
    const auto* tan = find_attr(prim, ossia::attribute_semantic::tangent);
    REQUIRE(tan);
    const float inv_len = 1.0f / std::sqrt(0.75f * 0.75f + 3.5f * 3.5f);
    const float tx = 0.75f * inv_len, ty = 3.5f * inv_len;
    const float* tg = attr_floats(prim, *tan);
    for(int v = 0; v < 3; v++)
    {
      INFO("tangent " << v);
      CHECK(tg[v * 4 + 0] == Approx(tx).margin(1e-6));
      CHECK(tg[v * 4 + 1] == Approx(ty).margin(1e-6));
      CHECK(tg[v * 4 + 2] == Approx(0.f).margin(1e-6));
      CHECK(tg[v * 4 + 3] == 1.0f); // exact: mikktspace writes +-1.0f
    }
  }

  SECTION("QuadMesh: one quad face triangulates to 2 triangles -> 6 vertices")
  {
    const auto* node = root_named(state, "Quad");
    REQUIRE(node);
    const auto* mesh = mesh_of(*node);
    REQUIRE(mesh);
    REQUIRE(mesh->primitives.size() == 1u);
    const auto& prim = mesh->primitives[0];
    check_primitive_invariants(prim);

    // A convex n-gon triangulates to n - 2 triangles: 4 - 2 = 2, and
    // FbxParser.cpp:438 multiplies by 3.
    CHECK(prim.vertex_count == 6u);
    REQUIRE(prim.attributes.size() == 4u);

    // The AABB is order-independent, so it is exact regardless of which
    // diagonal ufbx_triangulate_face picks: the four declared corners are
    // (0,0,0) (2,0,0) (2,2,0) (0,2,0).
    CHECK(prim.bounds.min[0] == 0.0f);
    CHECK(prim.bounds.min[1] == 0.0f);
    CHECK(prim.bounds.min[2] == 0.0f);
    CHECK(prim.bounds.max[0] == 2.0f);
    CHECK(prim.bounds.max[1] == 2.0f);
    CHECK(prim.bounds.max[2] == 0.0f);

    const auto* apos = find_attr(prim, ossia::attribute_semantic::position);
    const auto* anrm = find_attr(prim, ossia::attribute_semantic::normal);
    const auto* auv = find_attr(prim, ossia::attribute_semantic::texcoord0);
    REQUIRE(apos);
    REQUIRE(anrm); // negative control (FbxParser.cpp:459 -> if(false)) hits here
    REQUIRE(auv);
    const float* p = attr_floats(prim, *apos);
    const float* n = attr_floats(prim, *anrm);
    const float* uv = attr_floats(prim, *auv);

    // Corner -> multiplicity. Splitting a quad along a diagonal reuses exactly
    // the two endpoints of that diagonal, so of the six emitted vertices two
    // corners appear twice and two appear once — true for either diagonal, so
    // this does not depend on ufbx's triangulation choice.
    const float corners[4][3]
        = {{0, 0, 0}, {2, 0, 0}, {2, 2, 0}, {0, 2, 0}};
    int seen[4] = {0, 0, 0, 0};
    for(int v = 0; v < 6; v++)
    {
      INFO("quad vertex " << v);
      int which = -1;
      for(int c = 0; c < 4; c++)
        if(p[v * 3] == corners[c][0] && p[v * 3 + 1] == corners[c][1]
           && p[v * 3 + 2] == corners[c][2])
          which = c;
      REQUIRE(which >= 0); // every emitted position is a declared corner
      seen[which]++;

      // Every normal in the file is (0,0,1).
      CHECK(n[v * 3 + 0] == 0.0f);
      CHECK(n[v * 3 + 1] == 0.0f);
      CHECK(n[v * 3 + 2] == 1.0f);

      // The file pairs corner (x, y, 0) with UV (x/2, y/2) — (0,0)->(0,0),
      // (2,0)->(1,0), (2,2)->(1,1), (0,2)->(0,1). Halving is exact in binary,
      // so this per-vertex correspondence must hold exactly whatever order
      // triangulation emits: it pins that positions and UVs are indexed
      // through the SAME triangulated index (FbxParser.cpp:447-452 vs
      // :482-487), i.e. that the two streams are not desynchronized.
      CHECK(uv[v * 2 + 0] == p[v * 3 + 0] / 2.0f);
      CHECK(uv[v * 2 + 1] == p[v * 3 + 1] / 2.0f);
    }
    int twice = 0, once = 0;
    for(int c = 0; c < 4; c++)
    {
      INFO("corner " << c << " seen " << seen[c] << " times");
      CHECK(seen[c] >= 1);
      if(seen[c] == 2)
        twice++;
      else if(seen[c] == 1)
        once++;
    }
    CHECK(twice == 2);
    CHECK(once == 2);
  }

  SECTION("BareMesh: no normals in the file, so ufbx generates the face normal")
  {
    const auto* node = root_named(state, "Bare");
    REQUIRE(node);
    const auto* mesh = mesh_of(*node);
    REQUIRE(mesh);
    REQUIRE(mesh->primitives.size() == 1u);
    const auto& prim = mesh->primitives[0];
    check_primitive_invariants(prim);

    CHECK(prim.vertex_count == 3u);

    // Positions only in the file, and no UVs means the mikktspace branch at
    // FbxParser.cpp:538 cannot run, so exactly two attributes.
    REQUIRE(prim.attributes.size() == 2u);
    CHECK(prim.attributes[0].semantic == ossia::attribute_semantic::position);
    CHECK(prim.attributes[1].semantic == ossia::attribute_semantic::normal);
    CHECK_FALSE(find_attr(prim, ossia::attribute_semantic::texcoord0));
    CHECK_FALSE(find_attr(prim, ossia::attribute_semantic::tangent));

    const auto* apos = find_attr(prim, ossia::attribute_semantic::position);
    REQUIRE(apos);
    const float* p = attr_floats(prim, *apos);
    const float expected_pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    for(int i = 0; i < 9; i++)
    {
      INFO("position float " << i);
      CHECK(p[i] == expected_pos[i]);
    }

    // Closed form for the generated normal: the winding is (0,0,0) ->
    // (1,0,0) -> (0,1,0), so the face normal is
    // cross((1,0,0), (0,1,0)) = (0,0,1), already unit. There is no
    // LayerElementNormal in the fixture, so this value can ONLY come from
    // opts.generate_missing_normals (FbxParser.cpp:1025) — the narrow negative
    // control named in the header flips that flag and reddens exactly this.
    const auto* nrm = find_attr(prim, ossia::attribute_semantic::normal);
    REQUIRE(nrm);
    const float* n = attr_floats(prim, *nrm);
    for(int v = 0; v < 3; v++)
    {
      INFO("generated normal " << v);
      CHECK(n[v * 3 + 0] == 0.0f);
      CHECK(n[v * 3 + 1] == 0.0f);
      CHECK(n[v * 3 + 2] == 1.0f);
    }
  }
}

// ===========================================================================
// (b) the binary container decodes to the same streams as the ASCII one
// ===========================================================================

TEST_CASE(
    "FBX binary: a synthesized 7400 binary container decodes identically to "
    "the equivalent ASCII document",
    "[threedim][fbx][loader][binary]")
{
  TempDir tmp;
  const auto bin_bytes = binary_tri_fbx();
  const auto bin_path = tmp.write("tri_binary.fbx", bin_bytes);
  const auto ascii_path = tmp.write("tri_ascii.fbx", ascii_tri_fbx);

  // The binary magic, spelled out here so a broken writer fails on the
  // container rather than deep inside ufbx.
  REQUIRE(bin_bytes.size() > 27);
  CHECK(bin_bytes.compare(0, 20, "Kaydara FBX Binary  ") == 0);
  CHECK(bin_bytes[20] == '\0');
  CHECK(bin_bytes[21] == '\x1a');
  CHECK(bin_bytes[22] == '\0');
  uint32_t ver = 0;
  std::memcpy(&ver, bin_bytes.data() + 23, 4);
  CHECK(ver == 7400u);

  auto bin = load_fbx(bin_path);
  REQUIRE(bin); // the binary reader accepts the container at all
  REQUIRE(bin->m_raw_state);
  auto txt = load_fbx(ascii_path);
  REQUIRE(txt);
  REQUIRE(txt->m_raw_state);

  const auto* bnode = root_named(*bin->m_raw_state, "Tri");
  const auto* tnode = root_named(*txt->m_raw_state, "Tri");
  REQUIRE(bnode);
  REQUIRE(tnode);
  const auto* bmesh = mesh_of(*bnode);
  const auto* tmesh = mesh_of(*tnode);
  REQUIRE(bmesh);
  REQUIRE(tmesh);
  REQUIRE(bmesh->primitives.size() == 1u);
  REQUIRE(tmesh->primitives.size() == 1u);
  const auto& bp = bmesh->primitives[0];
  const auto& tp = tmesh->primitives[0];
  check_primitive_invariants(bp);
  check_primitive_invariants(tp);

  // Both documents declare the same 1 triangular face over the same three
  // vertices, so both must produce 3 vertices with position + generated
  // normal and nothing else. The container is the only difference.
  CHECK(bp.vertex_count == 3u);
  CHECK(tp.vertex_count == 3u);
  REQUIRE(bp.attributes.size() == 2u);
  REQUIRE(tp.attributes.size() == 2u);

  const float expected_pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  const auto* bap = find_attr(bp, ossia::attribute_semantic::position);
  const auto* tap = find_attr(tp, ossia::attribute_semantic::position);
  REQUIRE(bap);
  REQUIRE(tap);
  const float* bpos = attr_floats(bp, *bap);
  const float* tpos = attr_floats(tp, *tap);
  for(int i = 0; i < 9; i++)
  {
    INFO("position float " << i);
    // The ASCII decimal literals and the binary64 payload denote the same
    // dyadic rationals, so this is exact on both sides, and equal.
    CHECK(bpos[i] == expected_pos[i]);
    CHECK(tpos[i] == expected_pos[i]);
  }

  // Same generated face normal (0,0,1) from both containers — see the closed
  // form in the BareMesh section above.
  const auto* bn = find_attr(bp, ossia::attribute_semantic::normal);
  const auto* tn = find_attr(tp, ossia::attribute_semantic::normal);
  REQUIRE(bn);
  REQUIRE(tn);
  const float* bnf = attr_floats(bp, *bn);
  const float* tnf = attr_floats(tp, *tn);
  for(int i = 0; i < 9; i++)
  {
    INFO("normal float " << i);
    CHECK(bnf[i] == tnf[i]);
    CHECK(bnf[i] == (i % 3 == 2 ? 1.0f : 0.0f));
  }

  // Bounds: the unit right triangle in the XY plane.
  CHECK(bp.bounds.min[0] == 0.0f);
  CHECK(bp.bounds.min[1] == 0.0f);
  CHECK(bp.bounds.min[2] == 0.0f);
  CHECK(bp.bounds.max[0] == 1.0f);
  CHECK(bp.bounds.max[1] == 1.0f);
  CHECK(bp.bounds.max[2] == 0.0f);
}

// ===========================================================================
// (c) truncation matrices, fork-isolated
// ===========================================================================

#if defined(THREEDIM_HAS_FORK)
namespace
{
// Read every byte every published buffer claims to own. Under ASan this turns
// a loader that publishes a stream shorter than its own vertex_count into a
// hard failure in the child rather than a silent half-scene; without ASan it
// still catches a null or short buffer through the explicit size check.
// Returns via _exit(1) semantics: std::abort() is the "contract violated"
// signal the forked parent reads off waitpid().
[[nodiscard]] double walk_streams(const ossia::mesh_primitive& prim)
{
  double sink = 0;
  for(std::size_t i = 0; i < prim.attributes.size(); i++)
  {
    const auto& a = prim.attributes[i];
    const std::size_t bpv = bytes_per_vertex(a.format);
    if(bpv == 0)
      std::abort(); // unknown format published
    const auto* bd = buffer_of(prim, a);
    if(!bd || !bd->data)
      std::abort(); // attribute pointing at nothing
    if(bd->byte_size < int64_t(a.byte_offset)
                           + int64_t(prim.vertex_count) * int64_t(bpv))
      std::abort(); // shorter than the vertex_count it advertises
    const auto* base = reinterpret_cast<const unsigned char*>(bd->data.get());
    for(int64_t b = 0; b < bd->byte_size; b++)
      sink += double(base[b]);
  }
  return sink;
}

void walk_node(const ossia::scene_node& n, double& sink)
{
  if(!n.children)
    return;
  for(const auto& payload : *n.children)
  {
    if(auto* mc = ossia::get_if<ossia::mesh_component_ptr>(&payload))
    {
      if(*mc)
        for(const auto& prim : (*mc)->primitives)
          sink += walk_streams(prim);
    }
    else if(auto* child = ossia::get_if<ossia::scene_node_ptr>(&payload))
    {
      if(*child)
        walk_node(**child, sink);
    }
  }
}

// Child body for the "prefixes may be accepted, but never lie" contract: load
// the truncated file; if it is rejected we are done, and if it is accepted
// every stream it published must be fully readable.
void walk_or_abort(const std::string& path)
{
  auto parser = load_fbx(path);
  if(!parser || !parser->m_raw_state)
    return; // rejected — the acceptable outcome
  const auto& st = *parser->m_raw_state;
  if(!st.roots)
    return;
  volatile double sink = 0;
  for(const auto& r : *st.roots)
  {
    if(!r)
      std::abort(); // a null root in a published scene
    double local = 0;
    walk_node(*r, local);
    sink = sink + local;
  }
}

// Child body for the binary container, where acceptance itself is the
// violation: see the derivation in the fixture comment (absolute EndOffsets +
// a terminating NULL record at exactly EOF).
void reject_or_abort(const std::string& path)
{
  if(load_fbx(path))
    std::abort();
}

void truncation_matrix(
    const char* filename, const std::string& good, bool must_reject)
{
  TempDir tmp;

  // The untruncated fixture must load, or the truncations prove nothing.
  {
    const auto path = tmp.write(filename, good);
    auto parser = load_fbx(path);
    INFO("full fixture " << filename << " (" << good.size() << " bytes)");
    REQUIRE(parser);
    REQUIRE(parser->m_raw_state);
  }

  for(std::size_t n = 0; n < good.size(); n++)
  {
    const auto cut = std::string_view(good).substr(0, n);
    const auto path = tmp.write(filename, cut);
    const bool ok = threedim_test::survives([&] {
      if(must_reject)
        reject_or_abort(path);
      else
        walk_or_abort(path);
    });
    INFO(
        filename << " truncated to " << n << " of " << good.size()
                 << " bytes");
    CHECK(ok);
  }
}
} // namespace

TEST_CASE(
    "FBX: a truncated file is never a crash and never a half-scene",
    "[threedim][fbx][malformed][truncation]")
{
  // Fork-isolated (tests/fixtures/score_test/ForkProbe.hpp): an aborting or
  // segfaulting parser must be one red CHECK, not a dead suite. Same shape as
  // tests/unit/AssetLoaderFailure.cpp:604-615 and tests/unit/
  // GltfLoaderTest.cpp's glb_truncation_matrix.
  SECTION("binary 7400 container: EVERY strict prefix is rejected")
  {
    // Exact, and derived from the container rather than measured: each
    // record's EndOffset is an absolute file offset that a prefix pushes past
    // EOF, and the top level only terminates on a 13-byte NULL record that
    // sits at the very end of the file. There is no trailing padding in this
    // fixture, so no prefix can be a complete document.
    truncation_matrix("t_bin.fbx", binary_tri_fbx(), /*must_reject=*/true);
  }
  SECTION("ASCII 7500 scene: prefixes may parse, but never publish a lie")
  {
    // A text container has no length field, so a prefix that happens to cut on
    // a token boundary after the Objects block IS a well-formed document and
    // ufbx is right to accept it. The contract asserted is therefore the
    // honest one: no crash, and any scene that does come out must have every
    // buffer it advertises. This is the fixture with LayerElementNormal and
    // LayerElementUV arrays, so prefixes land in the middle of the normal and
    // UV payloads — which the positions-only ASCII matrix already in
    // tests/unit/AssetLoaderFailure.cpp:635 never reaches.
    truncation_matrix(
        "t_ascii.fbx", std::string(ascii_scene_fbx), /*must_reject=*/false);
  }
}
#endif

// ===========================================================================
// (d) an optional real DCC-exported asset
// ===========================================================================

TEST_CASE(
    "FBX real asset: a DCC-exported cube yields 36 triangulated vertices and "
    "six axis normals",
    "[threedim][fbx][real-assets]")
{
  // Spec §3.4 item 4 wants a small .fbx in the corpus; there is none today and
  // that script is not this test's to edit, so this leg is optional and SKIPs
  // with the exact command. The named file is a Blender 2.72 export of the
  // default cube that ships inside the vendored ufbx submodule's own sample
  // set, so satisfying this needs no network and commits nothing.
  const auto path = assets_dir() / "blender_272_cube_7400_binary.fbx";
  const auto bytes = read_file(path);
  if(!bytes)
    SKIP(
        "no real .fbx in the corpus - "
        "cp 3rdparty/ufbx/data/blender_272_cube_7400_binary.fbx "
        "~/ossia/threedim-assets/   (or add a pinned "
        "`fetch <url> <sha256> <name>` line for a small .fbx to "
        "tests/integration/threedim-render/fetch-real-assets.sh, per "
        "SPEC-SCENE-RENDER-TESTS.md §3.4 item 4)");

  // Independent header check: this must be a BINARY FBX, not something else
  // renamed. Magic per the container layout documented above.
  REQUIRE(bytes->size() > 27);
  REQUIRE(bytes->compare(0, 20, "Kaydara FBX Binary  ") == 0);

  auto parser = load_fbx(path.string());
  REQUIRE(parser);
  REQUIRE(parser->m_raw_state);
  const auto& state = *parser->m_raw_state;
  REQUIRE(state.roots);
  REQUIRE(state.roots->size() == 1u);

  const auto* mesh = mesh_of(*(*state.roots)[0]);
  REQUIRE(mesh);
  REQUIRE(mesh->primitives.size() == 1u);
  const auto& prim = mesh->primitives[0];
  check_primitive_invariants(prim);

  // Closed forms, from "a cube is six quad faces spanning [-1,1]^3":
  //   triangles = 6 faces x (4 - 2) = 12   -> vertices = 12 x 3 = 36
  // (FbxParser.cpp:438). Nothing here is read off a loader run.
  CHECK(prim.vertex_count == 36u);

  CHECK(prim.bounds.min[0] == -1.0f);
  CHECK(prim.bounds.min[1] == -1.0f);
  CHECK(prim.bounds.min[2] == -1.0f);
  CHECK(prim.bounds.max[0] == 1.0f);
  CHECK(prim.bounds.max[1] == 1.0f);
  CHECK(prim.bounds.max[2] == 1.0f);

  // Every emitted position is a cube corner: each coordinate is exactly +-1.
  const auto* apos = find_attr(prim, ossia::attribute_semantic::position);
  REQUIRE(apos);
  const float* p = attr_floats(prim, *apos);
  for(uint32_t v = 0; v < prim.vertex_count; v++)
  {
    INFO("cube vertex " << v);
    for(int c = 0; c < 3; c++)
      CHECK((p[v * 3 + c] == 1.0f || p[v * 3 + c] == -1.0f));
  }

  // Normals: a flat-shaded axis-aligned box has exactly the six unit axis
  // normals, one per face, and each face contributes 2 triangles = 6 emitted
  // vertices. So each of +-X / +-Y / +-Z must appear exactly 6 times and
  // nothing else may appear. NEGATIVE CONTROL: FbxParser.cpp:459 -> if(false)
  // reddens this block (and the attribute count below).
  const auto* nrm = find_attr(prim, ossia::attribute_semantic::normal);
  REQUIRE(nrm);
  const float* n = attr_floats(prim, *nrm);
  int axis_hits[6] = {0, 0, 0, 0, 0, 0}; // +X -X +Y -Y +Z -Z
  for(uint32_t v = 0; v < prim.vertex_count; v++)
  {
    INFO("cube normal " << v);
    const float x = n[v * 3], y = n[v * 3 + 1], z = n[v * 3 + 2];
    int which = -1;
    if(x == 1.0f && y == 0.0f && z == 0.0f)
      which = 0;
    else if(x == -1.0f && y == 0.0f && z == 0.0f)
      which = 1;
    else if(x == 0.0f && y == 1.0f && z == 0.0f)
      which = 2;
    else if(x == 0.0f && y == -1.0f && z == 0.0f)
      which = 3;
    else if(x == 0.0f && y == 0.0f && z == 1.0f)
      which = 4;
    else if(x == 0.0f && y == 0.0f && z == -1.0f)
      which = 5;
    REQUIRE(which >= 0);
    axis_hits[which]++;
  }
  for(int a = 0; a < 6; a++)
  {
    INFO("axis normal " << a);
    CHECK(axis_hits[a] == 6);
  }

  // This export carries no UV set, so the mikktspace branch at
  // FbxParser.cpp:538 does not fire: position + normal only. The invariant
  // "normals + UVs implies tangents", asserted in check_primitive_invariants,
  // is the part that holds for any other .fbx someone drops in here.
  CHECK(prim.attributes.size() == 2u);
}
