// P0-13 (SPEC-SCENE-RENDER-TESTS.md): an asset that fails to load publishes
// nothing — and says so.
//
// Contract under test, from Threedim/AssetLoader.cpp (verified):
//
//   AssetLoader::ins::asset_t::process(file_type tv)
//     :145-146   if(tv.filename.empty())  return {};
//     :253-260   built-in dispatch missed -> AssetLoaderRegistry::lookup(ext);
//                no registered parser -> `loaded` stays null
//     :262-263   if(!loaded)              return {};
//   i.e. a missing, empty-named, corrupt or unknown-extension asset yields an
//   EMPTY std::function. The avnd runtime applies nothing, so the loader keeps
//   whatever state it had (none, for a fresh instance).
//
//   AssetLoader::operator()()
//     :292-297   if(!m_parsed_state) { outputs.scene_out.scene.state = nullptr;
//                                      outputs.scene_out.dirty = 0; return; }
//     :302-303   success path: state = m_wrapped_state,
//                dirty = ossia::scene_port::dirty_transform
//
// "Publishes nothing" therefore means, concretely: scene_out.scene.state is
// the null shared_ptr and scene_out.dirty == 0. The downstream renderer draws
// a black frame.
//
// Dispatch table (AssetLoader.cpp :151-260): .fbx -> ufbx, .gltf/.glb ->
// fastgltf, .obj -> tinyobjloader (from tv.bytes), .ply -> miniply or the
// PrimitiveCloud PLY parser (from the file on disk, after a header sniff),
// .stl/.off -> vcglib (from the file on disk), .splat/.spz -> PrimitiveCloud
// binary codecs (from tv.bytes), anything else -> AssetLoaderRegistry.
//
// Today NOTHING on the failure path logs a warning, and the node has no error
// output port — the user gets a silent black frame. That silence is pinned as
// a defect below ([!shouldfail], "a failed asset load is diagnosable").
// Motivation: 24 real scores use Asset Loader and the corpus references
// assets by four different path syntaxes, including C:/Users/... paths played
// back on a Linux box — the missing file is the NORMAL case, not the edge.
//
// The ForkProbe truncation matrix feeds every byte-prefix of one small valid
// asset per supported format through the same entry points and requires (a)
// no crash and (b) the publishes-nothing / publishes-something dichotomy —
// a truncated ASCII file may legitimately still parse (fewer faces), but it
// must then publish a real state; a rejected one must publish null + dirty 0.

#include <score_test/ForkProbe.hpp>

#include <Threedim/AssetLoader.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QString>
#include <QtGlobal>

#include <load-spz.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace
{
// ---------------------------------------------------------------------------
// Temp-file plumbing (pattern: tests/threedim/GeometryLoaderFormats.cpp).
// One tag per process so a fixture cannot collide with a concurrently
// running copy of this executable.
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
    dir = fs::temp_directory_path() / ("score-threedim-assetfail-" + uniqueTag());
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

// ---------------------------------------------------------------------------
// Drives the shipped dispatch exactly as the avnd runtime does: process()
// with both the file's bytes and its name (the disk-reading parsers — ply,
// stl, off — only use the name; obj/gltf/glb/fbx/splat/spz use the bytes).
// ---------------------------------------------------------------------------
std::unique_ptr<Threedim::AssetLoader>
load_asset(std::string_view filename, std::string_view bytes)
{
  halp::text_file_view tv;
  tv.filename = filename;
  tv.bytes = bytes;
  auto apply = Threedim::AssetLoader::ins::asset_t::process(tv);
  if(!apply)
    return nullptr;
  auto loader = std::make_unique<Threedim::AssetLoader>();
  apply(*loader);
  return loader;
}

// The publishes-nothing contract of AssetLoader.cpp:292-297.
bool publishes_nothing(Threedim::AssetLoader& loader)
{
  loader();
  return loader.outputs.scene_out.scene.state == nullptr
         && loader.outputs.scene_out.dirty == 0;
}

// The success contract of AssetLoader.cpp:302-303.
bool publishes_scene(Threedim::AssetLoader& loader)
{
  loader();
  return loader.outputs.scene_out.scene.state != nullptr
         && loader.outputs.scene_out.scene.state == loader.m_wrapped_state
         && loader.outputs.scene_out.dirty == ossia::scene_port::dirty_transform;
}

// ---------------------------------------------------------------------------
// Fixtures — one minimal VALID asset per format the loader parses, generated
// in-test (patterns: tests/threedim/GeometryLoaderFormats.cpp fixtures,
// tests/unit/ThreedimLoaderTest.cpp make_glb / triangle_ascii_fbx / spz).
// Each full fixture is REQUIRE'd to load before its truncations are driven,
// so the matrix runs over inputs that are provably one-byte-short-of-valid.
// ---------------------------------------------------------------------------

constexpr std::string_view triangle_obj = R"(v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)";

// x/y/z + a face element: fails ply_is_splat_shaped(), so it takes the
// miniply mesh path (AssetLoader.cpp:188-199). Proven to load through
// AssetLoader in tests/unit/ThreedimLoaderTest.cpp ("xyz + face routes to
// the mesh path").
const std::string mesh_ply = "ply\nformat ascii 1.0\n"
                             "element vertex 3\n"
                             "property float x\nproperty float y\nproperty float z\n"
                             "element face 1\n"
                             "property list uchar int vertex_indices\n"
                             "end_header\n"
                             "0 0 0\n1 0 0\n0 1 0\n"
                             "3 0 1 2\n";

const std::string ascii_stl = R"(solid t
facet normal 0 0 1
  outer loop
    vertex 0 0 0
    vertex 1 0 0
    vertex 0 1 0
  endloop
endfacet
endsolid t
)";

const std::string ascii_off = R"(OFF
3 1 0
0 0 0
1 0 0
0 1 0
3 0 1 2
)";

// Minimal ASCII FBX 7.4 with one triangle (fixture proven against ufbx in
// tests/unit/ThreedimLoaderTest.cpp triangle_ascii_fbx()).
std::string triangle_ascii_fbx()
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

// A minimal valid GLB, synthesised byte-by-byte per the glTF 2.0 spec §4.4
// (Binary glTF container). Layout, byte for byte:
//
//   offset  0  uint32 magic   = 0x46546C67            'glTF'
//   offset  4  uint32 version = 2
//   offset  8  uint32 length  = 12 + (8 + jsonLen) + (8 + binLen), the
//                               EXACT total file size
//   offset 12  uint32 chunkLength = jsonLen  (4-byte aligned; the JSON text
//                                             is padded with 0x20 spaces,
//                                             the padding INCLUDED in the
//                                             declared length, per spec)
//   offset 16  uint32 chunkType   = 0x4E4F534A        'JSON'
//   offset 20  jsonLen bytes of JSON
//   then       uint32 chunkLength = binLen   (4-byte aligned; zero-padded,
//                                             padding included)
//              uint32 chunkType   = 0x004E4942        'BIN\0'
//              binLen bytes of binary payload
//
// The BIN payload is 42 real bytes padded to 44:
//   @0  three float3 positions (0,0,0)(1,0,0)(0,1,0)   36 bytes
//   @36 three uint16 indices 0,1,2                      6 bytes
// The JSON declares buffer 0 with byteLength 42 (unpadded size — the spec
// allows the GLB BIN chunk to be up to 3 bytes longer than the buffer),
// bufferView 0 = positions (target 34962 ARRAY_BUFFER), bufferView 1 =
// indices (target 34963 ELEMENT_ARRAY_BUFFER), a VEC3/float POSITION
// accessor with the min/max the spec requires for POSITION, and a
// SCALAR/uint16 (componentType 5123) index accessor. Asset version "2.0",
// one scene, one node, one mesh with one indexed primitive.
// Validity is additionally proven at runtime: the untruncated fixture is
// REQUIRE'd to parse through fastgltf before any truncation runs.
std::string triangle_glb()
{
  std::string bin;
  const float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  const uint16_t idx[3] = {0, 1, 2};
  for(float f : pos)
    put(bin, f);
  for(uint16_t i : idx)
    put(bin, i);

  // clang-format off
  std::string json = R"({"asset":{"version":"2.0"},)"
    R"("scene":0,"scenes":[{"nodes":[0]}],)"
    R"("nodes":[{"mesh":0,"name":"tri"}],)"
    R"("meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)"
    R"("buffers":[{"byteLength":42}],)"
    R"("bufferViews":[)"
      R"({"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},)"
      R"({"buffer":0,"byteOffset":36,"byteLength":6,"target":34963}],)"
    R"("accessors":[)"
      R"({"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,)"
        R"("type":"VEC3","min":[0,0,0],"max":[1,1,0]},)"
      R"({"bufferView":1,"byteOffset":0,"componentType":5123,"count":3,)"
        R"("type":"SCALAR"}]})";
  // clang-format on

  while(json.size() % 4)
    json.push_back(' ');
  while(bin.size() % 4)
    bin.push_back('\0');

  std::string out;
  put<uint32_t>(out, 0x46546C67); // 'glTF'
  put<uint32_t>(out, 2);
  put<uint32_t>(out, uint32_t(12 + 8 + json.size() + 8 + bin.size()));
  put<uint32_t>(out, uint32_t(json.size()));
  put<uint32_t>(out, 0x4E4F534A); // 'JSON'
  out += json;
  put<uint32_t>(out, uint32_t(bin.size()));
  put<uint32_t>(out, 0x004E4942); // 'BIN\0'
  out += bin;
  return out;
}

// Two Antimatter15 .splat rows: exactly 32 bytes per primitive — 3 float
// positions, 3 float scales, 4 uint8 rgba, 4 uint8 quaternion. Any size
// that is not a multiple of 32 is rejected by parse_splat_binary.
std::string two_splats()
{
  std::string out;
  const float rows[2][6]{
      {1.f, 2.f, 3.f, 0.5f, 0.5f, 0.5f},
      {-1.f, -2.f, -3.f, 0.25f, 0.25f, 0.25f}};
  for(const auto& r : rows)
  {
    for(float v : r)
      put(out, v);
    for(int i = 0; i < 8; ++i)
      out.push_back(char(128));
  }
  return out;
}

// A Niantic .spz v2 payload with two gaussians, produced by the very
// serializer (vendored 3rdparty/spz) whose decoder AssetLoader dispatches to
// — so the fixture is valid by construction.
std::string two_spz()
{
  spz::GaussianCloud c;
  c.numPoints = 2;
  c.shDegree = 0;
  c.positions = {0.5f, -1.25f, 2.f, -3.5f, 0.75f, -0.25f};
  c.scales = {-1.5f, -2.f, -2.5f, -3.f, -1.f, -2.f};
  c.rotations = {0.f, 0.f, 0.f, 1.f, 0.5f, 0.5f, 0.5f, 0.5f};
  c.alphas = {0.5f, -0.5f};
  c.colors = {0.25f, -0.5f, 0.75f, -1.f, 0.f, 1.f};

  spz::PackOptions po;
  po.version = 2;
  po.from = spz::CoordinateSystem::RDF;
  std::vector<uint8_t> packed;
  if(!spz::saveSpz(c, po, &packed))
    return {};
  return std::string(reinterpret_cast<const char*>(packed.data()), packed.size());
}

// Qt message capture for the diagnosability pin.
std::vector<std::string>& capturedMessages()
{
  static std::vector<std::string> v;
  return v;
}
void messageCapture(QtMsgType, const QMessageLogContext&, const QString& msg)
{
  capturedMessages().push_back(msg.toStdString());
}
} // namespace

// ===========================================================================
// The nullptr contract — CORRECT behaviour, asserted green.
// ===========================================================================

TEST_CASE(
    "an empty asset filename yields no apply function and the loader "
    "publishes nothing",
    "[threedim][assetloader][failure]")
{
  // AssetLoader.cpp:145-146.
  halp::text_file_view tv;
  tv.filename = std::string_view{};
  tv.bytes = std::string_view{};
  CHECK_FALSE(bool(Threedim::AssetLoader::ins::asset_t::process(tv)));

  // A loader that never received a scene publishes null + dirty 0
  // (AssetLoader.cpp:292-297) — every tick, not just once.
  Threedim::AssetLoader loader;
  CHECK(publishes_nothing(loader));
  CHECK(publishes_nothing(loader));
}

TEST_CASE(
    "a missing file publishes nothing, under every path syntax the corpus "
    "uses",
    "[threedim][assetloader][failure]")
{
  // The score corpus references assets through (at least) four path
  // syntaxes; on a Linux box a C:/Users/... path is simply a file that does
  // not exist. When the runtime cannot read the file it hands process() the
  // name with empty bytes; the disk-reading parsers (.ply/.stl/.off) then
  // fail their own open(), the byte-reading ones (.glb/.fbx/.obj) fail on
  // the empty view. Either way `loaded` stays null -> return {} at
  // AssetLoader.cpp:262-263.
  const std::string_view missing[] = {
      "/nonexistent/score-threedim/model.glb",        // absolute POSIX
      "C:/Users/someone/Desktop/model.glb",           // Windows, forward /
      "C:\\Users\\someone\\Desktop\\model.fbx",       // Windows, backslash
      "../relative/somewhere/model.obj",              // relative
      "/nonexistent/score-threedim/model.ply",        // disk-reading parsers
      "/nonexistent/score-threedim/model.stl",
      "/nonexistent/score-threedim/model.off",
      "C:/Users/someone/Documents/cloud.splat",
      "C:/Users/someone/Documents/cloud.spz",
  };
  for(auto path : missing)
  {
    INFO("path: " << path);
    CHECK_FALSE(bool(load_asset(path, {})));
  }

  Threedim::AssetLoader loader;
  CHECK(publishes_nothing(loader));
}

TEST_CASE(
    "a corrupt file publishes nothing for every dispatched format",
    "[threedim][assetloader][failure][malformed]")
{
  TempDir tmp;

  // 512 bytes of deterministic binary junk: no parser should see a header.
  std::string junk;
  for(int i = 0; i < 512; i++)
    junk.push_back(char(i * 37));

  const char* names[] = {"j.fbx",  "j.gltf",  "j.glb", "j.obj", "j.ply",
                         "j.stl",  "j.off",   "j.spz"};
  for(const char* n : names)
  {
    INFO("fixture: " << n);
    const auto path = tmp.write(n, junk);
    CHECK_FALSE(bool(load_asset(path, junk)));
  }

  // .splat separately: its only validity rule is size % 32 == 0, so junk of
  // 512 bytes would "parse". A 31-byte payload must be rejected.
  {
    const std::string odd(31, '\x42');
    const auto path = tmp.write("odd.splat", odd);
    CHECK_FALSE(bool(load_asset(path, odd)));
  }

  Threedim::AssetLoader loader;
  CHECK(publishes_nothing(loader));
}

TEST_CASE(
    "an unknown or absent extension publishes nothing even over valid bytes",
    "[threedim][assetloader][failure]")
{
  // Dispatch is by extension only (AssetLoader.cpp:151-260); an extension no
  // built-in matches and no addon registered falls off the registry lookup
  // at :253-260 and returns {} at :262-263 — even though the bytes are a
  // perfectly good OBJ.
  CHECK_FALSE(bool(load_asset("tri.model3d", triangle_obj)));
  CHECK_FALSE(bool(load_asset("triobj", triangle_obj)));   // dotless
  CHECK_FALSE(bool(load_asset("tri.", triangle_obj)));     // trailing dot
  CHECK_FALSE(bool(load_asset("tri.obj.bak", triangle_obj)));

  Threedim::AssetLoader loader;
  CHECK(publishes_nothing(loader));
}

TEST_CASE(
    "a failed load keeps the previously published scene instead of clearing "
    "it",
    "[threedim][assetloader][failure]")
{
  // process() returning {} means the runtime applies nothing, so a loader
  // that already holds a scene keeps publishing it — the failure mode for a
  // *replacement* asset is "old scene stays", not "black frame".
  TempDir tmp;
  const auto glb = triangle_glb();
  const auto path = tmp.write("tri.glb", glb);

  auto loader = load_asset(path, glb);
  REQUIRE(loader);
  REQUIRE(loader->m_parsed_state);
  REQUIRE(publishes_scene(*loader));
  const auto published = loader->outputs.scene_out.scene.state;

  // Now the user retargets the port at a file that does not exist.
  halp::text_file_view tv;
  tv.filename = "C:/Users/someone/Desktop/next.glb";
  tv.bytes = std::string_view{};
  auto apply = Threedim::AssetLoader::ins::asset_t::process(tv);
  CHECK_FALSE(bool(apply)); // nothing to apply...

  (*loader)(); // ...so the next tick still publishes the old scene.
  CHECK(loader->outputs.scene_out.scene.state == published);
  CHECK(loader->outputs.scene_out.scene.state != nullptr);
}

// ===========================================================================
// The diagnosability defect — pinned red.
// ===========================================================================

TEST_CASE(
    "a failed asset load is diagnosable",
    "[threedim][assetloader][failure][diagnosability][!shouldfail]")
{
  // DEFECT (silent failure). Every failure path in
  // AssetLoader::ins::asset_t::process (AssetLoader.cpp:145-146, :262-263)
  // returns an empty function without qWarning/qDebug, and the node's `outs`
  // struct (AssetLoader.hpp) carries only scene_out — no error/status port.
  // The user pointing the port at C:/Users/... on a Linux box gets a black
  // frame and NO message anywhere. 24 real scores use this node; the missing
  // file is the normal case, so the failure must be observable — either a
  // logged warning or a published error/status output. Until one exists this
  // case fails, and [!shouldfail] keeps it red-but-expected.

  // Channel 1: a diagnostic output port on the node itself. The probe must
  // be a template so the member accesses are dependent: a requires-expression
  // in a non-template context hard-errors on a missing member instead of
  // yielding false.
  constexpr bool has_error_port
      = []<typename O = Threedim::AssetLoader::outs>() constexpr {
          return requires(O& o) { o.error; } || requires(O& o) { o.status; }
                 || requires(O& o) { o.load_error; };
        }();
  CHECK(has_error_port);

  // Channel 2: a Qt log message on the failure path.
  capturedMessages().clear();
  const auto previous = qInstallMessageHandler(&messageCapture);
  {
    TempDir tmp;
    std::string junk(256, '\x51');
    (void)load_asset("C:/Users/someone/Desktop/missing.glb", {});
    (void)load_asset(tmp.write("junk.glb", junk), junk);
    (void)load_asset("tri.unknown-extension", triangle_obj);
  }
  qInstallMessageHandler(previous);
  CHECK(!capturedMessages().empty());
}

// ===========================================================================
// ForkProbe truncation matrix — every byte-prefix of one valid asset per
// format, fork-isolated so an aborting parser is one red CHECK, not a dead
// suite (pattern: tests/threedim/VoxelAssets.cpp "truncating a valid .vox
// never crashes the loader").
// ===========================================================================

#if defined(THREEDIM_HAS_FORK)
namespace
{
// Runs the full user-visible pipeline on one input and enforces the
// dichotomy: a load that succeeds must publish a real wrapped scene with
// dirty_transform; a load that fails must publish nullptr with dirty 0.
// abort()s on violation so the forked parent sees it as a non-survival.
void assert_contract_or_abort(std::string_view path, std::string_view bytes)
{
  halp::text_file_view tv;
  tv.filename = path;
  tv.bytes = bytes;
  auto apply = Threedim::AssetLoader::ins::asset_t::process(tv);
  Threedim::AssetLoader loader;
  if(apply)
    apply(loader);
  loader();
  const auto& out = loader.outputs.scene_out;
  if(apply)
  {
    if(out.scene.state == nullptr || out.scene.state != loader.m_wrapped_state
       || out.dirty != ossia::scene_port::dirty_transform)
      std::abort();
  }
  else
  {
    if(out.scene.state != nullptr || out.dirty != 0)
      std::abort();
  }
}

void truncation_matrix(const char* filename, const std::string& good)
{
  TempDir tmp;

  // The untruncated fixture must load: otherwise the truncations prove
  // nothing.
  {
    const auto path = tmp.write(filename, good);
    auto loader = load_asset(path, good);
    INFO("full fixture " << filename << " (" << good.size() << " bytes)");
    REQUIRE(loader);
    REQUIRE(loader->m_parsed_state);
    REQUIRE(publishes_scene(*loader));
  }

  // Every prefix, n = 0 (empty file) .. size-1. The truncated bytes are both
  // written to disk (for the parsers that read by filename) and passed as
  // the byte view (for the ones that parse tv.bytes) — exactly what the
  // runtime would hand process() for a half-written download.
  for(std::size_t n = 0; n < good.size(); n++)
  {
    const auto cut = std::string_view(good).substr(0, n);
    const auto path = tmp.write(filename, cut);
    const bool ok = threedim_test::survives(
        [&] { assert_contract_or_abort(path, cut); });
    INFO(
        filename << " truncated to " << n << " of " << good.size()
                 << " bytes");
    CHECK(ok);
  }
}
} // namespace

TEST_CASE(
    "truncating a valid asset never crashes the loader and never publishes a "
    "half-scene",
    "[threedim][assetloader][failure][malformed][truncation]")
{
  // One fixture per format AssetLoader itself parses. .gltf is not repeated:
  // the GLB fixture exercises the same fastgltf entry point plus the binary
  // container framing, which is where truncation bites.
  SECTION(".glb (fastgltf)") { truncation_matrix("t.glb", triangle_glb()); }
  SECTION(".obj (tinyobjloader)")
  {
    truncation_matrix("t.obj", std::string(triangle_obj));
  }
  SECTION(".ply (miniply)") { truncation_matrix("t.ply", mesh_ply); }
  SECTION(".stl (vcglib)") { truncation_matrix("t.stl", ascii_stl); }
  SECTION(".off (vcglib)") { truncation_matrix("t.off", ascii_off); }
  SECTION(".fbx (ufbx)") { truncation_matrix("t.fbx", triangle_ascii_fbx()); }
  SECTION(".splat (PrimitiveCloud)")
  {
    truncation_matrix("t.splat", two_splats());
  }
  SECTION(".spz (Niantic codec)")
  {
    const auto spz = two_spz();
    REQUIRE(!spz.empty());
    truncation_matrix("t.spz", spz);
  }
}
#endif
