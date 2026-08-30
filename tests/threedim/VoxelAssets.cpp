// Coverage for the MagicaVoxel ingestion path: Threedim/Vox.cpp
// (VoxPointCloudFromFile / VoxMeshFromFile / the 256-entry palette buffer) and
// Threedim/VoxelLoader.cpp (VoxelLoader, its vox_t file port and
// rebuild_geometry).
//
// Every fixture is synthesised byte-for-byte in the test, so the malformed
// cases are derived from a known-good file by truncating it or by rewriting one
// field. .vox is an untrusted binary format reached by a drag-and-drop, so the
// robustness cases are the point of this file.
//
// Malformed inputs are driven in a forked child: "does not crash" is only a
// real verdict if a crash cannot take the rest of the suite with it, and a
// SIGABRT/SIGSEGV inside the parser is exactly what we are looking for.

#include "ForkProbe.hpp"

#include <Threedim/VoxelLoader.hpp>
#include <Threedim/Vox.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using Catch::Approx;

namespace
{
struct VoxWriter
{
  std::string bytes;

  void tag(const char* t) { bytes.append(t, 4); }
  void u32(uint32_t v)
  {
    for(int i = 0; i < 4; i++)
      bytes.push_back(char((v >> (8 * i)) & 0xFF));
  }
  void u8(uint8_t v) { bytes.push_back(char(v)); }
};

struct Voxel
{
  uint8_t x, y, z, color;
};

// A minimal but fully valid .vox: header + MAIN + SIZE + XYZI. No scene-graph
// chunks, so ogt_vox synthesises a single identity-transform instance.
std::string makeVox(int sx, int sy, int sz, const std::vector<Voxel>& voxels)
{
  VoxWriter children;
  children.tag("SIZE");
  children.u32(12);
  children.u32(0);
  children.u32(sx);
  children.u32(sy);
  children.u32(sz);

  children.tag("XYZI");
  children.u32(4 + 4 * uint32_t(voxels.size()));
  children.u32(0);
  children.u32(uint32_t(voxels.size()));
  for(auto& v : voxels)
  {
    children.u8(v.x);
    children.u8(v.y);
    children.u8(v.z);
    children.u8(v.color);
  }

  VoxWriter f;
  f.tag("VOX ");
  f.u32(150);
  f.tag("MAIN");
  f.u32(0);
  f.u32(uint32_t(children.bytes.size()));
  f.bytes += children.bytes;
  return f.bytes;
}

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

  explicit TempFile(std::string_view contents, const char* suffix = ".vox")
  {
    static int counter = 0;
    path = std::filesystem::temp_directory_path()
           / ("score-threedim-vox-" + uniqueTag() + "-"
              + std::to_string(counter++) + suffix);
    std::ofstream f(path, std::ios::binary);
    f.write(contents.data(), std::streamsize(contents.size()));
  }
  ~TempFile()
  {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
  std::string str() const { return path.string(); }
};

} // namespace

TEST_CASE("a single voxel becomes one fully-exposed point", "[threedim][vox]")
{
  TempFile f{makeVox(1, 1, 1, {{0, 0, 0, 7}})};

  Threedim::float_vec buf, palette;
  auto meshes = Threedim::VoxPointCloudFromFile(f.str(), buf, palette);
  REQUIRE(meshes.size() == 1);

  const auto& m = meshes[0];
  CHECK(m.vertices == 1);
  CHECK(m.points);
  CHECK(m.pos_offset == 0);
  // position(3) + material_id(1) + face_mask(1) per voxel, non-interleaved.
  REQUIRE(buf.size() == 5);

  // The pivot is `size / 2` in *integer* arithmetic, so a 1-wide model is not
  // recentred: the voxel keeps its own-space centre.
  CHECK(buf[0] == Approx(0.5f));
  CHECK(buf[1] == Approx(0.5f));
  CHECK(buf[2] == Approx(0.5f));

  REQUIRE(m.extras.size() == 2);
  CHECK(m.extras[0].semantic == halp::attribute_semantic::material_id);
  CHECK(m.extras[0].offset == 3);
  CHECK(m.extras[0].components == 1);
  CHECK(m.extras[1].name == "face_mask");
  CHECK(m.extras[1].offset == 4);

  CHECK(buf[3] == Approx(7.f));
  // Bits 0..5 = +Z -Z +X -X +Y -Y; an isolated voxel exposes all six.
  CHECK(buf[4] == Approx(63.f));
}

TEST_CASE("face masks drop the shared face between two neighbours",
          "[threedim][vox]")
{
  // 2x1x1 grid, both cells solid: the +X face of voxel 0 and the -X face of
  // voxel 1 are interior and must be culled.
  TempFile f{makeVox(2, 1, 1, {{0, 0, 0, 3}, {1, 0, 0, 3}})};

  Threedim::float_vec buf, palette;
  auto meshes = Threedim::VoxPointCloudFromFile(f.str(), buf, palette);
  REQUIRE(meshes.size() == 1);
  REQUIRE(meshes[0].vertices == 2);
  REQUIRE(buf.size() == 10);

  const float* mask = buf.data() + 2 * 4;
  CHECK(mask[0] == Approx(63.f - 4.f)); // no +X
  CHECK(mask[1] == Approx(63.f - 8.f)); // no -X

  // The 2-wide axis IS recentred (2/2 == 1).
  CHECK(buf[0] == Approx(-0.5f));
  CHECK(buf[3] == Approx(0.5f));
}

TEST_CASE("the vox palette buffer has the documented std430 layout",
          "[threedim][vox]")
{
  TempFile f{makeVox(1, 1, 1, {{0, 0, 0, 1}})};

  Threedim::float_vec buf, palette;
  auto meshes = Threedim::VoxPointCloudFromFile(f.str(), buf, palette);
  REQUIRE(meshes.size() == 1);

  REQUIRE(palette.size() == std::size_t(Threedim::vox_palette_total_floats));
  CHECK(Threedim::vox_palette_total_floats == 256 * 8);
  CHECK(Threedim::vox_palette_byte_size == 256 * 8 * 4);

  for(int i = 0; i < 256; i++)
  {
    const float* e = palette.data() + i * 8;
    for(int c = 0; c < 4; c++)
    {
      CHECK(e[c] >= 0.f);
      CHECK(e[c] <= 1.f);
    }
    // No MATL chunks in the fixture: every entry takes the documented
    // defaults (metal 0, rough 0.5, emissive 0, ior 1.5).
    CHECK(e[4] == Approx(0.f));
    CHECK(e[5] == Approx(0.5f));
    CHECK(e[6] == Approx(0.f));
    CHECK(e[7] == Approx(1.5f));
  }

  // The material_id written per voxel indexes this palette directly; the
  // default MagicaVoxel palette is fully opaque.
  const int matid = int(buf[3]);
  REQUIRE(matid == 1);
  CHECK(palette[matid * 8 + 3] == Approx(1.f));
}

TEST_CASE("simple meshification emits two triangles per exposed face",
          "[threedim][vox]")
{
  TempFile f{makeVox(1, 1, 1, {{0, 0, 0, 5}})};

  Threedim::float_vec buf, palette;
  auto meshes = Threedim::VoxMeshFromFile(f.str(), buf, palette, 0);
  REQUIRE(meshes.size() == 1);

  const auto& m = meshes[0];
  // 6 cube faces * 2 triangles * 3 corners.
  CHECK(m.vertices == 36);
  CHECK_FALSE(m.points);
  CHECK(m.normals);
  CHECK(m.normal_offset == 36 * 3);
  REQUIRE(m.extras.size() == 1);
  CHECK(m.extras[0].semantic == halp::attribute_semantic::material_id);
  CHECK(m.extras[0].offset == 36 * 6);

  // position(3) + normal(3) + material_id(1), de-indexed.
  REQUIRE(buf.size() == 36 * 7);

  const float* nrm = buf.data() + 36 * 3;
  for(int i = 0; i < 36; i++)
  {
    const float x = nrm[3 * i], y = nrm[3 * i + 1], z = nrm[3 * i + 2];
    CHECK(std::sqrt(x * x + y * y + z * z) == Approx(1.f).margin(1e-5));
  }

  const float* mat = buf.data() + 36 * 6;
  for(int i = 0; i < 36; i++)
    CHECK(mat[i] == Approx(5.f));
}

TEST_CASE("greedy meshification merges coplanar same-colour faces",
          "[threedim][vox]")
{
  std::vector<Voxel> block;
  for(uint8_t z = 0; z < 2; z++)
    for(uint8_t y = 0; y < 2; y++)
      for(uint8_t x = 0; x < 2; x++)
        block.push_back({x, y, z, 9});
  TempFile f{makeVox(2, 2, 2, block)};

  Threedim::float_vec simpleBuf, simplePal;
  auto simple = Threedim::VoxMeshFromFile(f.str(), simpleBuf, simplePal, 0);
  Threedim::float_vec greedyBuf, greedyPal;
  auto greedy = Threedim::VoxMeshFromFile(f.str(), greedyBuf, greedyPal, 1);

  REQUIRE(simple.size() == 1);
  REQUIRE(greedy.size() == 1);
  CHECK(simple[0].vertices > 0);
  CHECK(greedy[0].vertices > 0);
  CHECK(greedy[0].vertices < simple[0].vertices);
  CHECK(simpleBuf.size() == std::size_t(simple[0].vertices) * 7);
  CHECK(greedyBuf.size() == std::size_t(greedy[0].vertices) * 7);
}

#if defined(THREEDIM_HAS_FORK)
TEST_CASE(
    "a model with no solid voxels yields nothing",
    "[threedim][vox]")
{
  // DEFECT (crash on a legitimate file). An empty model — an XYZI chunk with
  // a voxel count of 0 — is what MagicaVoxel writes for a cleared layer, so
  // it turns up in real assets. ogt_vox pushes a NULL into its model array for
  // it (unless k_read_scene_flags_keep_empty_models_instances is passed), and
  // its later model-dedup remap then trips
  //   ogt_assert(new_model_index != UINT32_MAX, "invalid model index ...")
  // which is a plain assert(): score aborts on load. With NDEBUG the assert
  // vanishes and the invalid index is used instead, which is worse.
  //
  // load_vox_scene() should pass k_read_scene_flags_keep_empty_models_instances
  // (and Vox.cpp should then skip NULL models), or replace ogt_assert with a
  // non-fatal handler.
  const std::string data = makeVox(4, 4, 4, {});
  TempFile f{data};
  const auto path = f.str();

  CHECK(threedim_test::survives([&] {
    Threedim::float_vec buf, palette;
    Threedim::VoxPointCloudFromFile(path, buf, palette);
    Threedim::float_vec mbuf, mpal;
    Threedim::VoxMeshFromFile(path, mbuf, mpal, 0);
  }));
}
#endif

TEST_CASE("missing and empty files are rejected", "[threedim][vox]")
{
  Threedim::float_vec buf, palette;
  CHECK(Threedim::VoxPointCloudFromFile(
            "/nonexistent/score-threedim/no-such.vox", buf, palette)
            .empty());
  CHECK(Threedim::VoxMeshFromFile(
            "/nonexistent/score-threedim/no-such.vox", buf, palette, 0)
            .empty());
  CHECK(palette.empty());

  TempFile empty{""};
  CHECK(Threedim::VoxPointCloudFromFile(empty.str(), buf, palette).empty());
  CHECK(Threedim::VoxMeshFromFile(empty.str(), buf, palette, 1).empty());
}

TEST_CASE("files that are not .vox at all are rejected", "[threedim][vox]")
{
  Threedim::float_vec buf, palette;

  TempFile text{"this is not a voxel file, it is a haiku\n"};
  CHECK(Threedim::VoxPointCloudFromFile(text.str(), buf, palette).empty());

  // Right magic, wrong version.
  VoxWriter w;
  w.tag("VOX ");
  w.u32(42);
  TempFile badver{w.bytes};
  CHECK(Threedim::VoxPointCloudFromFile(badver.str(), buf, palette).empty());

  // Header only: no MAIN, no models.
  VoxWriter h;
  h.tag("VOX ");
  h.u32(150);
  TempFile hdr{h.bytes};
  CHECK(Threedim::VoxPointCloudFromFile(hdr.str(), buf, palette).empty());
}

#if defined(THREEDIM_HAS_FORK)
TEST_CASE(
    "truncating a valid .vox never crashes the loader",
    "[threedim][vox][malformed]")
{
  // Every prefix of a well-formed file, byte by byte. A truncated download or
  // a half-written asset must be rejected, not parsed.
  //
  // DEFECT: 10 of the 75 prefixes abort. Prefixes 32..40 stop inside the SIZE
  // chunk, so ogt_vox reads zero-filled dimensions and trips
  //   ogt_assert(size_x && size_y && size_z, "SIZE chunk has zero size");
  // prefix 56 stops inside XYZI, leaving an empty model, and trips
  //   ogt_assert(new_model_index != UINT32_MAX, ...)
  // in the model-dedup remap. ogt_assert is a plain assert(), so on any build
  // without NDEBUG a truncated .vox aborts score; with NDEBUG the same inputs
  // proceed on unvalidated data instead. Threedim's load_vox_scene() should
  // either validate the buffer itself or define ogt_assert to something
  // non-fatal.
  const std::string good = makeVox(
      4, 4, 4, {{0, 0, 0, 1}, {1, 2, 3, 2}, {3, 3, 3, 3}, {2, 0, 1, 4}});

  for(std::size_t n = 1; n < good.size(); n++)
  {
    TempFile f{std::string_view(good).substr(0, n)};
    const auto path = f.str();
    const bool ok = threedim_test::survives([&] {
      Threedim::float_vec buf, palette;
      Threedim::VoxPointCloudFromFile(path, buf, palette);
      Threedim::float_vec mbuf, mpal;
      Threedim::VoxMeshFromFile(path, mbuf, mpal, 0);
      Threedim::VoxMeshFromFile(path, mbuf, mpal, 1);
    });
    INFO("truncated to " << n << " of " << good.size() << " bytes");
    CHECK(ok);
  }
}

namespace
{
bool voxSurvives(const std::string& data)
{
  TempFile f{data};
  const auto path = f.str();
  return threedim_test::survives([&] {
    Threedim::float_vec buf, palette;
    Threedim::VoxPointCloudFromFile(path, buf, palette);
    Threedim::float_vec mbuf, mpal;
    Threedim::VoxMeshFromFile(path, mbuf, mpal, 0);
  });
}
} // namespace

TEST_CASE("nonsense chunk lengths are absorbed", "[threedim][vox][malformed]")
{
  SECTION("XYZI claims far more voxels than it carries")
  {
    std::string data = makeVox(4, 4, 4, {{0, 0, 0, 1}});
    // The voxel count is the last uint32 before the payload.
    const std::size_t off = data.size() - 8;
    for(int i = 0; i < 4; i++)
      data[off + i] = char(0xFF);
    CHECK(voxSurvives(data));
  }

  SECTION("MAIN chunk with a wrong children length")
  {
    std::string data = makeVox(2, 2, 2, {{0, 0, 0, 1}});
    const std::size_t off = data.find("MAIN") + 8;
    for(int i = 0; i < 4; i++)
      data[off + i] = char(0xFF);
    CHECK(voxSurvives(data));
  }
}

TEST_CASE(
    "a corrupted SIZE or XYZI field never crashes the loader",
    "[threedim][vox][malformed]")
{
  // DEFECT. Four crafted single-field corruptions take the process down. The
  // last one is the serious one: it is a heap WRITE past the end of the
  // allocation, not an assert, so it is present in release builds too.
  {
    // ogt_assert_warn(inside_region, "invalid data in XYZI chunk")
    std::string data = makeVox(2, 2, 2, {{0, 0, 0, 1}});
    data[data.size() - 4] = char(200); // x = 200 in a 2-wide model
    INFO("voxel coordinate far outside the declared model");
    CHECK(voxSurvives(data));
  }
  {
    // ogt_assert(size_x && size_y && size_z, "SIZE chunk has zero size")
    INFO("SIZE 0 0 0");
    CHECK(voxSurvives(makeVox(0, 0, 0, {{0, 0, 0, 1}})));
  }
  {
    // voxel_count = size_x * size_y * size_z is computed in uint32 and wraps
    // to 0 for 2^30 cubed. ogt_vox then allocates sizeof(ogt_vox_model) + 0
    // bytes and writes the voxel payload into it:
    //   AddressSanitizer: heap-buffer-overflow, WRITE of size 1,
    //   0 bytes after a 24-byte region, at ogt_vox.h:1590.
    // A crafted .vox therefore corrupts the heap of any build. The SIZE
    // dimensions need a sanity bound before the multiply.
    INFO("SIZE 2^30 cubed: voxel_count overflows uint32 to zero");
    CHECK(voxSurvives(makeVox(0x40000000, 0x40000000, 0x40000000, {{0, 0, 0, 1}})));
  }
  {
    // ogt_assert(chunk_size == CHUNK_HEADER_LEN && chunk_child_size == 0, ...)
    std::string data = makeVox(2, 2, 2, {{0, 0, 0, 1}});
    const std::size_t off = data.find("SIZE") + 4;
    data[off] = char(0xFF);
    INFO("SIZE chunk content length = 255");
    CHECK(voxSurvives(data));
  }
}
#endif

TEST_CASE("VoxelLoader publishes a point cloud with its palette",
          "[threedim][voxelloader]")
{
  TempFile f{makeVox(2, 2, 2, {{0, 0, 0, 1}, {1, 1, 1, 2}})};

  Threedim::VoxelLoader loader;
  loader.inputs.mode.value = Threedim::VoxelLoader::PointCloud;

  halp::text_file_view tv;
  const auto path = f.str();
  tv.filename = path;
  auto apply = Threedim::VoxelLoader::ins::vox_t::process(tv);
  REQUIRE(apply);
  apply(loader);

  REQUIRE(loader.outputs.geometry.mesh.size() == 1);
  const auto& geom = loader.outputs.geometry.mesh[0];
  CHECK(loader.outputs.geometry.dirty_mesh);
  CHECK(geom.vertices == 2);
  CHECK(geom.topology == halp::primitive_topology::points);
  CHECK(geom.cull_mode == halp::cull_mode::none);

  // position + material_id + face_mask, one binding / attribute / input each.
  REQUIRE(geom.attributes.size() == 3);
  REQUIRE(geom.bindings.size() == 3);
  REQUIRE(geom.input.size() == 3);
  CHECK(geom.attributes[0].semantic == halp::attribute_semantic::position);
  CHECK(geom.attributes[1].semantic == halp::attribute_semantic::material_id);
  CHECK(geom.attributes[2].name == "face_mask");
  for(int i = 0; i < 3; i++)
  {
    CHECK(geom.attributes[i].binding == i);
    CHECK(geom.input[i].buffer == 0);
  }
  CHECK(geom.input[1].byte_offset == 2 * 3 * int(sizeof(float)));
  CHECK(geom.input[2].byte_offset == 2 * 4 * int(sizeof(float)));

  // The 256-entry palette rides along as a named auxiliary buffer.
  REQUIRE(geom.auxiliary.size() == 1);
  CHECK(geom.auxiliary[0].name == "vox_palette");
  REQUIRE(geom.auxiliary[0].buffer == 1);
  REQUIRE(geom.buffers.size() == 2);
  CHECK(geom.buffers[1].byte_size == Threedim::vox_palette_byte_size);
  CHECK(geom.auxiliary[0].byte_size == Threedim::vox_palette_byte_size);
}

TEST_CASE("VoxelLoader mesh modes map onto VoxMeshFromFile",
          "[threedim][voxelloader]")
{
  TempFile f{makeVox(1, 1, 1, {{0, 0, 0, 4}})};
  const auto path = f.str();

  for(auto mode :
      {Threedim::VoxelLoader::Mesh_Simple, Threedim::VoxelLoader::Mesh_Greedy})
  {
    Threedim::VoxelLoader loader;
    loader.inputs.mode.value = mode;

    halp::text_file_view tv;
    tv.filename = path;
    auto apply = Threedim::VoxelLoader::ins::vox_t::process(tv);
    REQUIRE(apply);
    apply(loader);

    REQUIRE(loader.outputs.geometry.mesh.size() == 1);
    const auto& geom = loader.outputs.geometry.mesh[0];
    CHECK(geom.topology == halp::primitive_topology::triangles);
    CHECK(geom.vertices == 36);
    // position + normal + material_id.
    REQUIRE(geom.attributes.size() == 3);
    CHECK(geom.attributes[1].semantic == halp::attribute_semantic::normal);
    CHECK(geom.attributes[2].semantic == halp::attribute_semantic::material_id);
  }
}

TEST_CASE("VoxelLoader keeps the previous geometry when a load fails",
          "[threedim][voxelloader]")
{
  TempFile good{makeVox(1, 1, 1, {{0, 0, 0, 4}})};
  TempFile bad{"not a vox file"};

  Threedim::VoxelLoader loader;
  loader.inputs.mode.value = Threedim::VoxelLoader::PointCloud;

  halp::text_file_view tv;
  const auto goodPath = good.str();
  tv.filename = goodPath;
  auto apply = Threedim::VoxelLoader::ins::vox_t::process(tv);
  REQUIRE(apply);
  apply(loader);
  REQUIRE(loader.outputs.geometry.mesh.size() == 1);
  const auto vertices = loader.outputs.geometry.mesh[0].vertices;

  const auto badPath = bad.str();
  tv.filename = badPath;
  auto applyBad = Threedim::VoxelLoader::ins::vox_t::process(tv);
  REQUIRE(applyBad);
  applyBad(loader);

  REQUIRE(loader.outputs.geometry.mesh.size() == 1);
  CHECK(loader.outputs.geometry.mesh[0].vertices == vertices);
}

TEST_CASE("VoxelLoader ignores an empty filename", "[threedim][voxelloader]")
{
  halp::text_file_view tv;
  CHECK_FALSE(bool(Threedim::VoxelLoader::ins::vox_t::process(tv)));
}
