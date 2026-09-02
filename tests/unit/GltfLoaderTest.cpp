// P1-5 (SPEC-SCENE-RENDER-TESTS.md): glTF and GLB load — CPU loader half only
// (P1-6, the render half, lives elsewhere). Closes gap G4: glTF/GLB had no
// test at any level against a real-world asset, despite Asset Loader being
// used by 24 real scores.
//
// Entry point under test — the exact static the avnd runtime calls
// (pattern: tests/unit/ThreedimLoaderTest.cpp):
//
//   Threedim::GltfParser::ins::gltf_t::process(file_type tv)
//     GltfParser.cpp:1067-1072  empty filename / missing file -> {}
//     GltfParser.cpp:1099       fastgltf::Options::LoadExternalBuffers
//     GltfParser.cpp:1108-1109  loadGltf(gltfFile, path.parent_path(), ...)
//                               -- external .bin URIs resolve relative to the
//                               DOCUMENT, not the process CWD (case (d))
//     GltfParser.cpp:683        vertex_count = POSITION accessor count
//     GltfParser.cpp:699-720    COLOR_0: vec4 passed through, vec3 padded to
//                               RGBA with alpha = 1 (case (b))
//     GltfParser.cpp:887        colors wired as attribute_semantic::color0,
//                               vertex_format::float4
//
// Cases, per the spec:
//  (a) Box.glb (Khronos glTF-Sample-Assets) yields ONE mesh with position +
//      normal streams and exactly 24 vertices / 36 indices. Closed forms and
//      provenance in the test body; the counts are cross-checked against the
//      GLB's own JSON chunk and against the parsed index stream — not blessed
//      from loader output.
//  (b) BoxVertexColors.glb additionally yields a color0 stream (float4,
//      padded from the file's VEC3 float COLOR_0 with alpha = 1).
//  (c) a truncated .glb at EVERY byte-prefix length is rejected without a
//      crash. Fork-isolated (score_test/ForkProbe.hpp) so an aborting parser
//      is one red CHECK, not a dead suite — the fork-matrix idiom of
//      tests/unit/AssetLoaderFailure.cpp and tests/threedim/VoxelAssets.cpp.
//      Runs everywhere: the fixture is a minimal valid GLB synthesized
//      byte-for-byte below; when the real Box.glb corpus is present its full
//      prefix matrix runs too.
//  (d) a .gltf with an external .bin resolves the buffer relative to the
//      document: JSON + bin pair synthesized byte-for-byte in a
//      QTemporaryDir, loaded positions compared for exact float equality
//      with the bytes we wrote.
//
// Real assets: Box.glb / BoxVertexColors.glb are fetched (pinned URL +
// sha256, licence CC0 1.0) by
//   tests/integration/threedim-render/fetch-real-assets.sh
// into ~/ossia/threedim-assets (the script's default destination). They are
// never committed; the corpus-dependent cases SKIP when they are absent.
//
// Registration (tests/unit/CMakeLists.txt): mirror test_unit_threedim_loaders
// / test_unit_threedim_asset_loader_failure — SOURCES GltfLoaderTest.cpp +
// ${_threedim_hidden}, PLUGINS score_plugin_threedim score_plugin_gfx
// score_plugin_avnd, LIBS test_unit_threedim_3rdparty fastgltf spz
// ${QT_PREFIX}::Gui, plus the vcglib/eigen SYSTEM include dirs.
//
// Negative controls (each makes exactly one case red):
//  (a) drop `sp.vertex_count = uint32_t(a->count)` at GltfParser.cpp:683
//      -> the 24-vertex assertion goes red.
//  (b) drop the `get_accessor("COLOR_0")` branch at GltfParser.cpp:699-720
//      -> the color0-stream assertion goes red.
//  (c) remove the accessor-range sweep at GltfParser.cpp:1122-1127
//      -> prefixes that keep the JSON but truncate the BIN chunk can OOB-read
//      under ASAN; the forked child dies non-zero -> red.
//  (d) pass `{}` instead of `path.parent_path()` at GltfParser.cpp:1109
//      -> the external .bin no longer resolves -> red.

#include <Threedim/GltfParser.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QTemporaryDir>

#include <score_test/ForkProbe.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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

namespace
{
// ---------------------------------------------------------------------------
// Temp-file plumbing (pattern: tests/unit/AssetLoaderFailure.cpp /
// tests/threedim/GeometryLoaderFormats.cpp). One tag per process so a fixture
// cannot collide with a concurrently running copy of this executable.
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
    dir = fs::temp_directory_path() / ("score-threedim-gltf-" + uniqueTag());
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
// fetch-real-assets.sh ("${1:-$HOME/ossia/threedim-assets}").
fs::path assets_dir()
{
  const char* home = ::getenv("HOME");
  return fs::path{home ? home : "/home/jcelerier"} / "ossia"
         / "threedim-assets";
}

constexpr const char* fetch_hint
    = "run tests/integration/threedim-render/fetch-real-assets.sh to fetch "
      "the corpus into ~/ossia/threedim-assets";

// ---------------------------------------------------------------------------
// Driving idiom of tests/unit/ThreedimLoaderTest.cpp: process() returns an
// apply-lambda (or an empty function on rejection); applying it to a fresh
// parser instance stores the scene in m_raw_state.
// ---------------------------------------------------------------------------
std::unique_ptr<Threedim::GltfParser> load_gltf(std::string_view filename)
{
  halp::text_file_view tv;
  tv.filename = filename;
  tv.bytes = std::string_view{}; // GltfParser reads from disk (FromPath)
  auto apply = Threedim::GltfParser::ins::gltf_t::process(tv);
  if(!apply)
    return nullptr;
  auto parser = std::make_unique<Threedim::GltfParser>();
  apply(*parser);
  return parser;
}

// ---------------------------------------------------------------------------
// scene_state inspection helpers (pattern: tests/unit/ThreedimLoaderTest.cpp)
// ---------------------------------------------------------------------------
const ossia::mesh_component* find_first_mesh(const ossia::scene_node& n)
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

const ossia::mesh_component* find_first_mesh(const ossia::scene_state& s)
{
  if(!s.roots)
    return nullptr;
  for(const auto& r : *s.roots)
    if(auto* m = find_first_mesh(*r))
      return m;
  return nullptr;
}

int count_meshes(const ossia::scene_node& n)
{
  int count = 0;
  if(!n.children)
    return 0;
  for(const auto& payload : *n.children)
  {
    if(ossia::get_if<ossia::mesh_component_ptr>(&payload))
      count++;
    if(auto* child = ossia::get_if<ossia::scene_node_ptr>(&payload))
      count += count_meshes(**child);
  }
  return count;
}

int count_meshes(const ossia::scene_state& s)
{
  int count = 0;
  if(!s.roots)
    return 0;
  for(const auto& r : *s.roots)
    count += count_meshes(*r);
  return count;
}

const ossia::vertex_attribute*
find_attr(const ossia::mesh_primitive& p, ossia::attribute_semantic sem)
{
  for(const auto& a : p.attributes)
    if(a.semantic == sem)
      return &a;
  return nullptr;
}

// Resolve the CPU float pointer for an attribute (non-GPU-resident buffers).
const float*
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

// Extract the JSON chunk of a GLB per glTF 2.0 spec §4.4: 12-byte header
// (magic 'glTF', version, total length), then chunk 0 header (uint32 length,
// uint32 type 'JSON') at offset 12, JSON text at offset 20. Little-endian
// reads — the same host-LE assumption the make_glb writers across this test
// suite already make.
std::optional<std::string> glb_json_chunk(const std::string& bytes)
{
  if(bytes.size() < 20)
    return std::nullopt;
  auto rd32 = [&](std::size_t off) {
    uint32_t v{};
    std::memcpy(&v, bytes.data() + off, 4);
    return v;
  };
  if(rd32(0) != 0x46546C67u) // 'glTF'
    return std::nullopt;
  const uint32_t json_len = rd32(12);
  if(rd32(16) != 0x4E4F534Au) // 'JSON'
    return std::nullopt;
  if(bytes.size() < 20 + std::size_t(json_len))
    return std::nullopt;
  return bytes.substr(20, json_len);
}

} // namespace

// ===========================================================================
// (c) fixture + fork matrix helpers — unix only (ForkProbe)
// ===========================================================================

#if defined(THREEDIM_HAS_FORK)
namespace
{
// ---------------------------------------------------------------------------
// Case (c) fixture: a minimal valid GLB, synthesized byte-for-byte per the
// glTF 2.0 spec §4.4 (Binary glTF container) — the exact layout documented
// and proven in tests/unit/AssetLoaderFailure.cpp triangle_glb(). Byte map:
//
//   offset  0  uint32 magic   = 0x46546C67            'glTF'
//   offset  4  uint32 version = 2
//   offset  8  uint32 length  = 12 + (8 + jsonLen) + (8 + binLen)
//                               == the EXACT total file size, so EVERY strict
//                               byte-prefix of the file is spec-invalid
//   offset 12  uint32 chunkLength = jsonLen (JSON padded to 4B with 0x20)
//   offset 16  uint32 chunkType   = 0x4E4F534A        'JSON'
//   offset 20  jsonLen bytes of JSON
//   then       uint32 chunkLength = binLen (zero-padded to 4B)
//              uint32 chunkType   = 0x004E4942        'BIN\0'
//              binLen bytes of binary payload
//
// BIN payload: 42 real bytes padded to 44 —
//   @0  three float3 positions (0,0,0)(1,0,0)(0,1,0)  36 bytes
//   @36 three uint16 indices 0,1,2                     6 bytes
// The JSON declares buffer 0 byteLength 42 (unpadded; the spec allows the
// BIN chunk up to 3 bytes longer), a VEC3/float POSITION accessor with the
// min/max the spec requires for POSITION, and a SCALAR/u16 index accessor.
// Validity is proven at runtime: the untruncated fixture is REQUIRE'd to
// parse before any truncation is driven.
// ---------------------------------------------------------------------------
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

} // namespace
#endif

// ===========================================================================
// (a) Box.glb — one mesh, position + normal streams, 24 vertices
// ===========================================================================

TEST_CASE(
    "glTF real asset: Box.glb yields one mesh with position+normal and "
    "exactly 24 vertices",
    "[threedim][gltf][real-assets]")
{
  const auto path = assets_dir() / "Box.glb";
  const auto bytes = read_file(path);
  if(!bytes)
    SKIP("Box.glb not present — " << fetch_hint);

  // -------------------------------------------------------------------------
  // Closed forms, with provenance:
  //
  //   vertices = 6 faces x 4 corners = 24
  //
  // A glTF vertex is a (position, normal, ...) tuple. The Khronos Box is
  // flat-shaded — one normal per face — so no cube corner can be shared
  // between faces: each of the 6 faces carries its own 4 corners.
  // Equivalently: 8 geometric corners x 3 incident faces each = 24.
  //
  //   indices = 6 faces x 2 triangles x 3 = 36
  //
  // Source: Khronos glTF-Sample-Assets, Models/Box/glTF-Binary/Box.glb
  // (URL + sha256 ed52f719... pinned in tests/integration/threedim-render/
  // fetch-real-assets.sh; licence CC0 1.0). The model's own JSON chunk
  // declares accessor 0 = SCALAR u16 "count":36 (indices), accessors 1 and 2
  // = VEC3 float "count":24 (NORMAL, POSITION), POSITION min/max
  // [-0.5,-0.5,-0.5]..[0.5,0.5,0.5] — a unit cube centred on the origin.
  //
  // Both counts are cross-checked below against the file's own JSON chunk
  // (the GLB's minified JSON contains the literal accessor declarations) and
  // against the parsed index stream (max index used == vertex_count - 1) —
  // the loader's output is not blessed as its own oracle.
  // -------------------------------------------------------------------------
  constexpr uint32_t box_vertices = 6 * 4;     // 24
  constexpr uint32_t box_indices = 6 * 2 * 3;  // 36

  const auto json = glb_json_chunk(*bytes);
  REQUIRE(json);
  // The accessor declarations, straight from the file (minified JSON).
  REQUIRE(json->find("\"count\":24") != std::string::npos);
  REQUIRE(json->find("\"count\":36") != std::string::npos);
  REQUIRE(json->find("\"POSITION\"") != std::string::npos);
  REQUIRE(json->find("\"NORMAL\"") != std::string::npos);
  // Box.glb carries no texcoords and no vertex colours.
  CHECK(json->find("\"TEXCOORD_0\"") == std::string::npos);
  CHECK(json->find("\"COLOR_0\"") == std::string::npos);

  auto parser = load_gltf(path.string());
  REQUIRE(parser);
  REQUIRE(parser->m_raw_state);
  const auto& state = *parser->m_raw_state;

  // ONE mesh in the whole scene (Box.glb: root node with a Z-up->Y-up
  // matrix, one child node carrying mesh 0 with one primitive).
  REQUIRE(state.roots);
  REQUIRE(state.roots->size() == 1);
  CHECK(count_meshes(state) == 1);

  const auto* mesh = find_first_mesh(state);
  REQUIRE(mesh);
  REQUIRE(mesh->primitives.size() == 1);
  const auto& prim = mesh->primitives[0];

  CHECK(prim.vertex_count == box_vertices);
  CHECK(prim.index_count == box_indices);
  // glTF u16 indices are widened to u32 by the parser.
  CHECK(prim.index_type == ossia::index_format::uint32);
  CHECK(prim.topology == ossia::primitive_topology::triangles);

  // position + normal streams; no texcoords in the file, and tangent
  // generation needs normals + uvs, so exactly these two attributes.
  const auto* pos = find_attr(prim, ossia::attribute_semantic::position);
  const auto* nrm = find_attr(prim, ossia::attribute_semantic::normal);
  REQUIRE(pos);
  REQUIRE(nrm);
  CHECK(pos->format == ossia::vertex_format::float3);
  CHECK(nrm->format == ossia::vertex_format::float3);
  CHECK(find_attr(prim, ossia::attribute_semantic::texcoord0) == nullptr);
  CHECK(find_attr(prim, ossia::attribute_semantic::tangent) == nullptr);
  CHECK(find_attr(prim, ossia::attribute_semantic::color0) == nullptr);
  CHECK(prim.attributes.size() == 2);
  // Non-interleaved on our side: one buffer per attribute (the file itself
  // interleaves POSITION and NORMAL in one byteStride-12 bufferView; the
  // parser decodes to separate streams).
  CHECK(prim.vertex_buffers.size() == 2);

  // AABB from the POSITION stream: the file's declared min/max, exactly
  // (+-0.5 is exactly representable, and the parser recomputes the AABB by
  // walking the same decoded floats).
  CHECK(prim.bounds.min[0] == -0.5f);
  CHECK(prim.bounds.min[1] == -0.5f);
  CHECK(prim.bounds.min[2] == -0.5f);
  CHECK(prim.bounds.max[0] == 0.5f);
  CHECK(prim.bounds.max[1] == 0.5f);
  CHECK(prim.bounds.max[2] == 0.5f);

  // Index-stream cross-check of the vertex count: 36 u32 indices whose used
  // range is exactly [0, 23] — so 24 is what the file itself indexes, not
  // just what the loader happened to output.
  REQUIRE(prim.index_buffer);
  const auto* ibd
      = ossia::get_if<ossia::buffer_data>(&prim.index_buffer->resource);
  REQUIRE(ibd);
  REQUIRE(ibd->byte_size == int64_t(box_indices * sizeof(uint32_t)));
  const uint32_t* idx = reinterpret_cast<const uint32_t*>(ibd->data.get());
  uint32_t min_idx = idx[0], max_idx = idx[0];
  for(uint32_t i = 0; i < box_indices; i++)
  {
    min_idx = std::min(min_idx, idx[i]);
    max_idx = std::max(max_idx, idx[i]);
  }
  CHECK(min_idx == 0);
  CHECK(max_idx == box_vertices - 1);
}

// ===========================================================================
// (b) BoxVertexColors.glb — additionally a color0 stream
// ===========================================================================

TEST_CASE(
    "glTF real asset: BoxVertexColors.glb additionally yields a color0 "
    "stream",
    "[threedim][gltf][real-assets]")
{
  const auto path = assets_dir() / "BoxVertexColors.glb";
  const auto bytes = read_file(path);
  if(!bytes)
    SKIP("BoxVertexColors.glb not present — " << fetch_hint);

  // Same box topology as case (a): 6 x 4 = 24 vertices, 6 x 2 x 3 = 36
  // indices (provenance there). This model's JSON chunk declares FOUR
  // accessors: SCALAR u16 count 36 (indices) and three VEC3 float count 24 —
  // POSITION (min/max [0,0,0]..[1,1,1]), NORMAL, and COLOR_0 as VEC3 FLOAT.
  // Source: Khronos glTF-Sample-Assets, Models/BoxVertexColors/glTF-Binary/
  // BoxVertexColors.glb (sha256 9c48227f... pinned in fetch-real-assets.sh).
  //
  // COLOR_0 being VEC3 in the file pins the parser's RGBA padding path
  // (GltfParser.cpp:704-719): the stream must surface as float4 with
  // alpha == 1 for every vertex.
  constexpr uint32_t box_vertices = 6 * 4;    // 24
  constexpr uint32_t box_indices = 6 * 2 * 3; // 36

  const auto json = glb_json_chunk(*bytes);
  REQUIRE(json);
  REQUIRE(json->find("\"COLOR_0\"") != std::string::npos);
  REQUIRE(json->find("\"count\":24") != std::string::npos);
  REQUIRE(json->find("\"count\":36") != std::string::npos);

  auto parser = load_gltf(path.string());
  REQUIRE(parser);
  REQUIRE(parser->m_raw_state);
  const auto& state = *parser->m_raw_state;

  CHECK(count_meshes(state) == 1);
  const auto* mesh = find_first_mesh(state);
  REQUIRE(mesh);
  REQUIRE(mesh->primitives.size() == 1);
  const auto& prim = mesh->primitives[0];

  CHECK(prim.vertex_count == box_vertices);
  CHECK(prim.index_count == box_indices);

  const auto* pos = find_attr(prim, ossia::attribute_semantic::position);
  const auto* nrm = find_attr(prim, ossia::attribute_semantic::normal);
  const auto* c0 = find_attr(prim, ossia::attribute_semantic::color0);
  REQUIRE(pos);
  REQUIRE(nrm);
  REQUIRE(c0); // negative control: drop GltfParser.cpp:699-720 -> red here
  CHECK(prim.attributes.size() == 3);
  CHECK(prim.vertex_buffers.size() == 3);

  // VEC3 float colours padded to RGBA (GltfParser.cpp:709-716).
  CHECK(c0->format == ossia::vertex_format::float4);
  const float* cols = attr_floats(prim, *c0);
  for(uint32_t v = 0; v < box_vertices; v++)
  {
    INFO("vertex " << v);
    CHECK(cols[v * 4 + 3] == 1.0f); // padded alpha
    // The file declares COLOR_0 min [0,0,0] / max [1,1,1]; every decoded
    // component must sit in that range.
    for(int comp = 0; comp < 3; comp++)
    {
      CHECK(cols[v * 4 + comp] >= 0.0f);
      CHECK(cols[v * 4 + comp] <= 1.0f);
    }
  }
  // First vertex colour, exactly as stored in the BIN chunk: (0, 0, 0);
  // second vertex (1, 0, 0). Byte-verified against the shipped asset.
  CHECK(cols[0] == 0.0f);
  CHECK(cols[1] == 0.0f);
  CHECK(cols[2] == 0.0f);
  CHECK(cols[4] == 1.0f);
  CHECK(cols[5] == 0.0f);
  CHECK(cols[6] == 0.0f);

  // Position bounds: this box spans [0,1]^3 (unlike Box.glb's +-0.5).
  CHECK(prim.bounds.min[0] == 0.0f);
  CHECK(prim.bounds.min[1] == 0.0f);
  CHECK(prim.bounds.min[2] == 0.0f);
  CHECK(prim.bounds.max[0] == 1.0f);
  CHECK(prim.bounds.max[1] == 1.0f);
  CHECK(prim.bounds.max[2] == 1.0f);
}

// ===========================================================================
// (c) truncated .glb at EVERY prefix length — rejected, no crash
// ===========================================================================

#if defined(THREEDIM_HAS_FORK)
namespace
{
// Child body: a strict byte-prefix of a valid GLB must be rejected outright.
// The GLB header's `length` field declares the exact total file size, so any
// prefix is spec-invalid; and even a prefix keeping the whole JSON chunk
// leaves buffer 0 without its BIN chunk data, which fastgltf validation
// refuses. abort() on acceptance so the forked parent sees non-survival;
// crashes (signal or sanitizer exit) also read as non-survival.
void reject_or_abort(std::string_view path)
{
  halp::text_file_view tv;
  tv.filename = path;
  tv.bytes = std::string_view{};
  if(Threedim::GltfParser::ins::gltf_t::process(tv))
    std::abort();
}

void glb_truncation_matrix(const char* filename, const std::string& good)
{
  TempDir tmp;

  // The untruncated fixture must parse: otherwise the truncations prove
  // nothing.
  {
    const auto path = tmp.write(filename, good);
    auto parser = load_gltf(path);
    INFO("full fixture " << filename << " (" << good.size() << " bytes)");
    REQUIRE(parser);
    REQUIRE(parser->m_raw_state);
  }

  // Every prefix, n = 0 (empty file) .. size-1.
  for(std::size_t n = 0; n < good.size(); n++)
  {
    const auto cut = std::string_view(good).substr(0, n);
    const auto path = tmp.write(filename, cut);
    const bool ok
        = threedim_test::survives([&] { reject_or_abort(path); });
    INFO(
        filename << " truncated to " << n << " of " << good.size()
                 << " bytes");
    CHECK(ok);
  }
}
} // namespace

TEST_CASE(
    "a .glb truncated at every prefix length is rejected without a crash",
    "[threedim][gltf][malformed][truncation]")
{
  SECTION("synthesized minimal GLB (runs everywhere)")
  {
    glb_truncation_matrix("tri.glb", triangle_glb());
  }
  SECTION("real Box.glb (when the corpus is fetched)")
  {
    const auto bytes = read_file(assets_dir() / "Box.glb");
    if(!bytes)
      SKIP("Box.glb not present — " << fetch_hint);
    glb_truncation_matrix("Box.glb", *bytes);
  }
}
#endif

// ===========================================================================
// (d) .gltf with an external .bin — resolved relative to the DOCUMENT
// ===========================================================================

TEST_CASE(
    "a .gltf resolves its external .bin relative to the document, not the "
    "CWD",
    "[threedim][gltf]")
{
  // GltfParser.cpp:1108-1109 hands fastgltf `path.parent_path()` as the base
  // directory, with Options::LoadExternalBuffers (GltfParser.cpp:1099). The
  // fixture puts the .bin in a subdirectory NEXT TO the document and refers
  // to it with the relative URI "data/tri.bin"; the test never chdir()s, so
  // resolution against the CWD would not find the file.
  QTemporaryDir tdir;
  REQUIRE(tdir.isValid());
  const fs::path root = fs::path(tdir.path().toStdString());
  fs::create_directories(root / "data");

  // The .bin, byte for byte: three float3 positions then three u16 indices,
  // 42 bytes. Every float below is exactly representable in binary32, and
  // the loader hands back the decoded IEEE bytes unchanged — so the
  // comparisons at the end are EXACT equality, not Approx.
  const float px[9] = {0.25f, -1.5f, 3.75f, 1.0f, 2.0f, -0.5f,
                       -2.25f, 0.125f, 8.5f};
  std::string bin;
  for(float f : px)
    put(bin, f);
  const uint16_t idx[3] = {0, 1, 2};
  for(uint16_t i : idx)
    put(bin, i);
  REQUIRE(bin.size() == 42);
  {
    std::ofstream f(root / "data" / "tri.bin", std::ios::binary);
    f.write(bin.data(), std::streamsize(bin.size()));
  }

  // min/max computed from the nine floats above, per component — glTF
  // requires them on POSITION accessors.
  // clang-format off
  const std::string json = R"({
"asset":{"version":"2.0"},
"scene":0,
"scenes":[{"nodes":[0]}],
"nodes":[{"mesh":0,"name":"tri"}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],
"buffers":[{"byteLength":42,"uri":"data/tri.bin"}],
"bufferViews":[
 {"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},
 {"buffer":0,"byteOffset":36,"byteLength":6,"target":34963}],
"accessors":[
 {"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[-2.25,-1.5,-0.5],"max":[1.0,2.0,8.5]},
 {"bufferView":1,"byteOffset":0,"componentType":5123,"count":3,"type":"SCALAR"}]
})";
  // clang-format on
  const fs::path doc = root / "model.gltf";
  {
    std::ofstream f(doc, std::ios::binary);
    f.write(json.data(), std::streamsize(json.size()));
  }

  SECTION("the buffer resolves and the positions are byte-exact")
  {
    auto parser = load_gltf(doc.string());
    REQUIRE(parser); // negative control: pass {} instead of
                     // path.parent_path() at GltfParser.cpp:1109 -> red
    REQUIRE(parser->m_raw_state);

    const auto* mesh = find_first_mesh(*parser->m_raw_state);
    REQUIRE(mesh);
    REQUIRE(mesh->primitives.size() == 1);
    const auto& prim = mesh->primitives[0];
    CHECK(prim.vertex_count == 3);
    CHECK(prim.index_count == 3);
    CHECK(prim.attributes.size() == 1); // POSITION only

    const auto* pos = find_attr(prim, ossia::attribute_semantic::position);
    REQUIRE(pos);
    const float* p = attr_floats(prim, *pos);
    for(int i = 0; i < 9; i++)
    {
      INFO("float " << i);
      CHECK(p[i] == px[i]); // exact — these are the bytes we wrote
    }

    // Indices came from the same external .bin, widened to u32.
    REQUIRE(prim.index_buffer);
    const auto* ibd
        = ossia::get_if<ossia::buffer_data>(&prim.index_buffer->resource);
    REQUIRE(ibd);
    const uint32_t* ix = reinterpret_cast<const uint32_t*>(ibd->data.get());
    CHECK(ix[0] == 0);
    CHECK(ix[1] == 1);
    CHECK(ix[2] == 2);
  }

  SECTION("control: the same document with a missing .bin is rejected")
  {
    // Proves the success half really depended on resolving OUR .bin: with
    // the target file absent the parser must reject, not zero-fill.
    auto broken = json;
    const auto at = broken.find("data/tri.bin");
    REQUIRE(at != std::string::npos);
    broken.replace(at, std::strlen("data/tri.bin"), "data/gone.bin");
    const fs::path doc2 = root / "broken.gltf";
    {
      std::ofstream f(doc2, std::ios::binary);
      f.write(broken.data(), std::streamsize(broken.size()));
    }
    CHECK_FALSE(load_gltf(doc2.string()));
  }
}
