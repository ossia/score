// Unit tests for the PrimitiveCloud point-cloud / Gaussian-splat parsers
// (Threedim/PrimitiveCloud/): PlyParser, SplatBinary, SpzCodec,
// FormatOverride, SceneFromCloud. Pure parsing logic, no app context.
//
// All inputs are generated in-test: tiny ASCII / binary-LE / binary-BE
// PLY files, hand-packed 32-byte .splat rows, and .spz buffers encoded
// with the vendored Niantic spz library (spz::saveSpz round-trip).
// Corrupt / truncated / empty inputs assert the graceful-failure paths
// (nullptr, no crash — ASAN validates no OOB in the parsers).

#include <Threedim/PrimitiveCloud/FormatOverride.hpp>
#include <Threedim/PrimitiveCloud/PlyParser.hpp>
#include <Threedim/PrimitiveCloud/SceneFromCloud.hpp>
#include <Threedim/PrimitiveCloud/SplatBinary.hpp>
#include <Threedim/PrimitiveCloud/SpzCodec.hpp>

#include <ossia/detail/variant.hpp>

#include <QDir>
#include <QTemporaryDir>

#include <load-spz.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using Catch::Approx;
namespace PC = Threedim::PrimitiveCloud;

namespace
{

// ---------------------------------------------------------------- helpers

struct TempTree
{
  QTemporaryDir dir;

  std::string write(const char* name, const std::string& bytes)
  {
    REQUIRE(dir.isValid());
    const auto path = dir.filePath(QString::fromUtf8(name)).toStdString();
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    REQUIRE(f.good());
    f.write(bytes.data(), (std::streamsize)bytes.size());
    f.close();
    REQUIRE(f.good());
    return path;
  }
};

void append_f32_le(std::string& out, float v)
{
  static_assert(sizeof(float) == 4);
  char b[4];
  std::memcpy(b, &v, 4); // test hosts are little-endian (x86_64 CI)
  out.append(b, 4);
}

void append_f32_be(std::string& out, float v)
{
  char b[4];
  std::memcpy(b, &v, 4);
  std::swap(b[0], b[3]);
  std::swap(b[1], b[2]);
  out.append(b, 4);
}

void append_u8(std::string& out, uint8_t v)
{
  out.push_back((char)v);
}

const ossia::buffer_data* raw_bytes(const ossia::primitive_cloud_component_ptr& c)
{
  REQUIRE(c);
  REQUIRE(c->raw_data);
  auto* bd = ossia::get_if<ossia::buffer_data>(&c->raw_data->resource);
  REQUIRE(bd);
  REQUIRE(bd->data);
  return bd;
}

float read_f32(const ossia::buffer_data* bd, size_t byte_offset)
{
  REQUIRE((int64_t)(byte_offset + 4) <= bd->byte_size);
  float v;
  std::memcpy(&v, (const uint8_t*)bd->data.get() + byte_offset, 4);
  return v;
}

uint8_t read_u8(const ossia::buffer_data* bd, size_t byte_offset)
{
  REQUIRE((int64_t)(byte_offset + 1) <= bd->byte_size);
  return ((const uint8_t*)bd->data.get())[byte_offset];
}

// 15-column all-float 3DGS-classic-shaped vertex schema, used by the
// ascii / binary-LE / binary-BE cases below. Column order:
//   x y z f_dc_0 f_dc_1 f_dc_2 f_rest_0 opacity
//   scale_0 scale_1 scale_2 rot_0 rot_1 rot_2 rot_3
constexpr int kClassicCols = 15;
constexpr float kClassicRows[2][kClassicCols] = {
    {1.f, 2.f, 3.f, 0.5f, 0.25f, -0.5f, 0.125f, 1.5f, -1.f, -2.f, -3.f, 1.f, 0.f,
     0.f, 0.f},
    {-4.f, 5.f, -6.f, 0.1f, 0.2f, 0.3f, -0.125f, -1.5f, -0.5f, -1.5f, -2.5f, 0.f,
     1.f, 0.f, 0.f},
};

std::string classic_header(const char* format)
{
  std::string h;
  h += "ply\n";
  h += "format ";
  h += format;
  h += " 1.0\n";
  h += "element vertex 2\n";
  for(const char* col :
      {"x", "y", "z", "f_dc_0", "f_dc_1", "f_dc_2", "f_rest_0", "opacity",
       "scale_0", "scale_1", "scale_2", "rot_0", "rot_1", "rot_2", "rot_3"})
  {
    h += "property float ";
    h += col;
    h += "\n";
  }
  h += "end_header\n";
  return h;
}

void check_classic_cloud(const ossia::primitive_cloud_component_ptr& cloud)
{
  REQUIRE(cloud);
  CHECK(cloud->primitive_count == 2);
  CHECK(cloud->row_stride == kClassicCols * 4);
  CHECK(cloud->topology == ossia::primitive_topology::points);
  CHECK(cloud->format_id == "3dgs.classic");
  CHECK(cloud->struct_type_name == "Splat3DGS");
  CHECK(cloud->stable_id != 0);

  const auto* bd = raw_bytes(cloud);
  REQUIRE(bd->byte_size == 2 * kClassicCols * 4);
  for(int r = 0; r < 2; ++r)
    for(int c = 0; c < kClassicCols; ++c)
      CHECK(read_f32(bd, r * kClassicCols * 4 + c * 4) == kClassicRows[r][c]);

  CHECK_FALSE(cloud->bounds.empty());
  CHECK(cloud->bounds.min[0] == -4.f);
  CHECK(cloud->bounds.min[1] == 2.f);
  CHECK(cloud->bounds.min[2] == -6.f);
  CHECK(cloud->bounds.max[0] == 1.f);
  CHECK(cloud->bounds.max[1] == 5.f);
  CHECK(cloud->bounds.max[2] == 3.f);
}

ossia::primitive_cloud_component_ptr make_cloud(std::string format_id = {})
{
  auto storage = std::shared_ptr<uint8_t[]>(new uint8_t[16]());
  auto br = std::make_shared<ossia::buffer_resource>();
  br->resource = ossia::buffer_data{
      .data = std::shared_ptr<const void>(storage, storage.get()),
      .byte_size = 16,
      .usage_hint = ossia::buffer_data::usage::storage_buffer};
  auto c = std::make_shared<ossia::primitive_cloud_component>();
  c->raw_data = std::move(br);
  c->row_stride = 16;
  c->primitive_count = 1;
  c->format_id = std::move(format_id);
  c->stable_id = ossia::mint_stable_id();
  return c;
}

} // namespace

// ================================================================ PlyParser

TEST_CASE("PLY: ascii 3DGS-classic file parses with exact values", "[ply]")
{
  TempTree t;
  std::string body;
  for(const auto& row : kClassicRows)
  {
    for(int c = 0; c < kClassicCols; ++c)
    {
      body += std::to_string(row[c]);
      body += (c + 1 == kClassicCols) ? "\n" : " ";
    }
  }
  const auto path = t.write("classic_ascii.ply", classic_header("ascii") + body);

  CHECK(PC::ply_is_splat_shaped(path));
  check_classic_cloud(PC::parse_ply(path));
}

TEST_CASE("PLY: binary_little_endian 3DGS-classic file parses with exact values",
          "[ply]")
{
  TempTree t;
  std::string body;
  for(const auto& row : kClassicRows)
    for(float v : row)
      append_f32_le(body, v);
  const auto path
      = t.write("classic_le.ply", classic_header("binary_little_endian") + body);

  CHECK(PC::ply_is_splat_shaped(path));
  check_classic_cloud(PC::parse_ply(path));
}

TEST_CASE("PLY: binary_big_endian 3DGS-classic file parses with exact values",
          "[ply]")
{
  TempTree t;
  std::string body;
  for(const auto& row : kClassicRows)
    for(float v : row)
      append_f32_be(body, v);
  const auto path
      = t.write("classic_be.ply", classic_header("binary_big_endian") + body);

  CHECK(PC::ply_is_splat_shaped(path));
  check_classic_cloud(PC::parse_ply(path));
}

TEST_CASE("PLY: mesh-shaped file (xyz + face) is rejected", "[ply]")
{
  TempTree t;
  const auto path = t.write(
      "mesh.ply",
      "ply\n"
      "format ascii 1.0\n"
      "element vertex 3\n"
      "property float x\n"
      "property float y\n"
      "property float z\n"
      "element face 1\n"
      "property list uchar int vertex_indices\n"
      "end_header\n"
      "0 0 0\n"
      "1 0 0\n"
      "0 1 0\n"
      "3 0 1 2\n");

  CHECK_FALSE(PC::ply_is_splat_shaped(path));
  CHECK(PC::parse_ply(path) == nullptr);
}

TEST_CASE("PLY: mesh columns + face + one extra column is splat-shaped", "[ply]")
{
  TempTree t;
  const auto path = t.write(
      "meshextra.ply",
      "ply\n"
      "format ascii 1.0\n"
      "element vertex 1\n"
      "property float x\n"
      "property float y\n"
      "property float z\n"
      "property float opacity\n"
      "element face 1\n"
      "property list uchar int vertex_indices\n"
      "end_header\n"
      "1 2 3 0.5\n"
      "3 0 0 0\n");

  CHECK(PC::ply_is_splat_shaped(path));
  auto cloud = PC::parse_ply(path);
  REQUIRE(cloud);
  CHECK(cloud->primitive_count == 1);
  CHECK(cloud->row_stride == 16);
  CHECK(cloud->format_id.empty()); // opacity alone is not the 3dgs fingerprint
  CHECK(cloud->struct_type_name.empty());
}

TEST_CASE("PLY: mixed uchar/float columns follow natural alignment", "[ply]")
{
  // red(u8) @0, x/y/z(f32) @4/8/12, green(u8) @16 -> stride 20 (align 4).
  // No face element => splat-shaped even though all columns are mesh-set.
  TempTree t;
  const auto path = t.write(
      "mixed.ply",
      "ply\n"
      "format ascii 1.0\n"
      "element vertex 2\n"
      "property uchar red\n"
      "property float x\n"
      "property float y\n"
      "property float z\n"
      "property uchar green\n"
      "end_header\n"
      "255 1.5 2.5 3.5 128\n"
      "7 -1 -2 -3 9\n");

  CHECK(PC::ply_is_splat_shaped(path));
  auto cloud = PC::parse_ply(path);
  REQUIRE(cloud);
  CHECK(cloud->primitive_count == 2);
  CHECK(cloud->row_stride == 20);
  CHECK(cloud->format_id.empty());

  const auto* bd = raw_bytes(cloud);
  REQUIRE(bd->byte_size == 40);
  CHECK(read_u8(bd, 0) == 255);
  CHECK(read_f32(bd, 4) == 1.5f);
  CHECK(read_f32(bd, 8) == 2.5f);
  CHECK(read_f32(bd, 12) == 3.5f);
  CHECK(read_u8(bd, 16) == 128);
  CHECK(read_u8(bd, 20) == 7);
  CHECK(read_f32(bd, 24) == -1.f);
  CHECK(read_f32(bd, 28) == -2.f);
  CHECK(read_f32(bd, 32) == -3.f);
  CHECK(read_u8(bd, 36) == 9);

  // AABB computed from the float x/y/z columns.
  CHECK(cloud->bounds.min[0] == -1.f);
  CHECK(cloud->bounds.max[2] == 3.5f);
}

TEST_CASE("PLY: double columns get 8-byte alignment; non-float xyz skips AABB",
          "[ply]")
{
  // x/y/z(double) @0/8/16, w(f32) @24 -> row_offset 28, align 8 -> stride 32.
  TempTree t;
  const auto path = t.write(
      "doubles.ply",
      "ply\n"
      "format ascii 1.0\n"
      "element vertex 1\n"
      "property double x\n"
      "property double y\n"
      "property double z\n"
      "property float custom\n"
      "end_header\n"
      "1.5 -2.5 3.25 0.5\n");

  CHECK(PC::ply_is_splat_shaped(path)); // "custom" is an extra column
  auto cloud = PC::parse_ply(path);
  REQUIRE(cloud);
  CHECK(cloud->primitive_count == 1);
  CHECK(cloud->row_stride == 32);

  const auto* bd = raw_bytes(cloud);
  REQUIRE(bd->byte_size == 32);
  double x, y, z;
  std::memcpy(&x, (const uint8_t*)bd->data.get() + 0, 8);
  std::memcpy(&y, (const uint8_t*)bd->data.get() + 8, 8);
  std::memcpy(&z, (const uint8_t*)bd->data.get() + 16, 8);
  CHECK(x == 1.5);
  CHECK(y == -2.5);
  CHECK(z == 3.25);
  CHECK(read_f32(bd, 24) == 0.5f);

  // xyz are doubles, not floats -> AABB path skipped -> empty sentinel.
  CHECK(cloud->bounds.empty());
}

TEST_CASE("PLY: short/int integer columns are sized and aligned", "[ply]")
{
  // x/y/z(f32) @0/4/8, s16 @12, u16 @14, i32 @16, u32 @20 -> stride 24.
  TempTree t;
  const auto path = t.write(
      "ints.ply",
      "ply\n"
      "format ascii 1.0\n"
      "element vertex 1\n"
      "property float x\n"
      "property float y\n"
      "property float z\n"
      "property short a\n"
      "property ushort b\n"
      "property int c\n"
      "property uint d\n"
      "end_header\n"
      "1 2 3 -12345 54321 -100000 4000000000\n");

  auto cloud = PC::parse_ply(path);
  REQUIRE(cloud);
  CHECK(cloud->primitive_count == 1);
  CHECK(cloud->row_stride == 24);

  const auto* bd = raw_bytes(cloud);
  REQUIRE(bd->byte_size == 24);
  int16_t a;
  uint16_t b;
  int32_t c;
  uint32_t d;
  std::memcpy(&a, (const uint8_t*)bd->data.get() + 12, 2);
  std::memcpy(&b, (const uint8_t*)bd->data.get() + 14, 2);
  std::memcpy(&c, (const uint8_t*)bd->data.get() + 16, 4);
  std::memcpy(&d, (const uint8_t*)bd->data.get() + 20, 4);
  CHECK(a == -12345);
  CHECK(b == 54321);
  CHECK(c == -100000);
  CHECK(d == 4000000000u);
}

TEST_CASE("PLY: list columns inside the vertex element are skipped", "[ply]")
{
  // The list column sits BEFORE the extra scalar so the splat-shape
  // sniffer's list-skip branch is exercised too.
  TempTree t;
  const auto path = t.write(
      "listcol.ply",
      "ply\n"
      "format ascii 1.0\n"
      "element vertex 2\n"
      "property float x\n"
      "property float y\n"
      "property float z\n"
      "property list uchar float extra\n"
      "property float opacity\n"
      "end_header\n"
      "1 2 3 2 10 20 0.5\n"
      "4 5 6 1 30 0.25\n");

  auto cloud = PC::parse_ply(path);
  REQUIRE(cloud);
  CHECK(cloud->primitive_count == 2);
  CHECK(cloud->row_stride == 16); // x y z opacity only; list skipped

  const auto* bd = raw_bytes(cloud);
  CHECK(read_f32(bd, 0) == 1.f);
  CHECK(read_f32(bd, 12) == 0.5f);
  CHECK(read_f32(bd, 16) == 4.f);
  CHECK(read_f32(bd, 28) == 0.25f);
}

TEST_CASE("PLY: vertex element with only list columns is rejected", "[ply]")
{
  TempTree t;
  const auto path = t.write(
      "listonly.ply",
      "ply\n"
      "format ascii 1.0\n"
      "element vertex 1\n"
      "property list uchar float extra\n"
      "end_header\n"
      "2 1 2\n");

  CHECK(PC::ply_is_splat_shaped(path)); // no face element
  CHECK(PC::parse_ply(path) == nullptr); // but no scalar column to pack
}

TEST_CASE("PLY: error paths return false / nullptr without crashing", "[ply]")
{
  TempTree t;

  SECTION("nonexistent path")
  {
    const auto path = t.dir.filePath("does_not_exist.ply").toStdString();
    CHECK_FALSE(PC::ply_is_splat_shaped(path));
    CHECK(PC::parse_ply(path) == nullptr);
  }
  SECTION("empty file")
  {
    const auto path = t.write("empty.ply", "");
    CHECK_FALSE(PC::ply_is_splat_shaped(path));
    CHECK(PC::parse_ply(path) == nullptr);
  }
  SECTION("garbage header")
  {
    const auto path = t.write("garbage.ply", "not a ply file at all\n\x01\x02\x03");
    CHECK_FALSE(PC::ply_is_splat_shaped(path));
    CHECK(PC::parse_ply(path) == nullptr);
  }
  SECTION("truncated binary payload")
  {
    // Header declares 2 rows; provide half a row of data.
    std::string body;
    append_f32_le(body, 1.f);
    append_f32_le(body, 2.f);
    const auto path
        = t.write("trunc.ply", classic_header("binary_little_endian") + body);
    CHECK(PC::ply_is_splat_shaped(path)); // header itself is fine
    CHECK(PC::parse_ply(path) == nullptr);
  }
  SECTION("ascii row with non-numeric junk")
  {
    const auto path = t.write(
        "junk.ply",
        "ply\n"
        "format ascii 1.0\n"
        "element vertex 1\n"
        "property float x\n"
        "property float y\n"
        "property float z\n"
        "property float opacity\n"
        "end_header\n"
        "foo bar baz quux\n");
    CHECK(PC::parse_ply(path) == nullptr);
  }
  SECTION("zero vertices")
  {
    const auto path = t.write(
        "zero.ply",
        "ply\n"
        "format ascii 1.0\n"
        "element vertex 0\n"
        "property float x\n"
        "property float y\n"
        "property float z\n"
        "property float opacity\n"
        "end_header\n");
    CHECK(PC::parse_ply(path) == nullptr);
  }
  SECTION("no vertex element at all")
  {
    const auto path = t.write(
        "novertex.ply",
        "ply\n"
        "format ascii 1.0\n"
        "element blob 1\n"
        "property float value\n"
        "end_header\n"
        "42\n");
    // No face element and no standard columns -> shaped like a splat...
    CHECK(PC::ply_is_splat_shaped(path));
    // ...but there is no vertex element to load.
    CHECK(PC::parse_ply(path) == nullptr);
  }
}

// ============================================================== SplatBinary

namespace
{
struct SplatRow
{
  float pos[3];
  float scale[3];
  uint8_t rgba[4];
  uint8_t rot[4];
};

std::string pack_splat_rows(std::initializer_list<SplatRow> rows)
{
  std::string out;
  for(const auto& r : rows)
  {
    for(float v : r.pos)
      append_f32_le(out, v);
    for(float v : r.scale)
      append_f32_le(out, v);
    for(uint8_t v : r.rgba)
      append_u8(out, v);
    for(uint8_t v : r.rot)
      append_u8(out, v);
  }
  return out;
}
} // namespace

TEST_CASE(".splat: two-row buffer parses with verbatim passthrough", "[splat]")
{
  const std::string bytes = pack_splat_rows({
      {{1.f, 2.f, 3.f}, {0.5f, 0.25f, 0.125f}, {255, 128, 64, 32}, {255, 128, 0, 64}},
      {{-1.f, -2.f, -3.f}, {1.f, 1.f, 1.f}, {0, 0, 0, 0}, {128, 128, 128, 128}},
  });
  REQUIRE(bytes.size() == 64);

  auto cloud = PC::parse_splat_binary(bytes);
  REQUIRE(cloud);
  CHECK(cloud->primitive_count == 2);
  CHECK(cloud->row_stride == 32);
  CHECK(cloud->topology == ossia::primitive_topology::points);
  CHECK(cloud->format_id == "3dgs.splat-binary");
  CHECK(cloud->struct_type_name.empty());
  CHECK(cloud->stable_id != 0);

  const auto* bd = raw_bytes(cloud);
  REQUIRE(bd->byte_size == 64);
  CHECK(std::memcmp(bd->data.get(), bytes.data(), bytes.size()) == 0);

  // Spot-check field decoding at the documented offsets.
  CHECK(read_f32(bd, 0) == 1.f);   // row0 pos.x
  CHECK(read_f32(bd, 12) == 0.5f); // row0 scale.x
  CHECK(read_u8(bd, 24) == 255);   // row0 color.r
  CHECK(read_u8(bd, 28) == 255);   // row0 rot[0]
  CHECK(read_f32(bd, 32 + 8) == -3.f); // row1 pos.z

  CHECK_FALSE(cloud->bounds.empty());
  CHECK(cloud->bounds.min[0] == -1.f);
  CHECK(cloud->bounds.min[1] == -2.f);
  CHECK(cloud->bounds.min[2] == -3.f);
  CHECK(cloud->bounds.max[0] == 1.f);
  CHECK(cloud->bounds.max[1] == 2.f);
  CHECK(cloud->bounds.max[2] == 3.f);

  // The parser must own a copy: mutating the input afterwards must not
  // affect the parsed buffer.
  std::string mutated = bytes;
  mutated[0] = (char)0xFF;
  CHECK(std::memcmp(bd->data.get(), bytes.data(), bytes.size()) == 0);
}

TEST_CASE(".splat: single row is accepted", "[splat]")
{
  const std::string bytes = pack_splat_rows(
      {{{0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {10, 20, 30, 40}, {128, 128, 128, 255}}});
  auto cloud = PC::parse_splat_binary(bytes);
  REQUIRE(cloud);
  CHECK(cloud->primitive_count == 1);
  // Single point at origin: min == max == 0.
  CHECK_FALSE(cloud->bounds.empty());
  CHECK(cloud->bounds.min[0] == 0.f);
  CHECK(cloud->bounds.max[0] == 0.f);
}

TEST_CASE(".splat: invalid sizes are rejected", "[splat]")
{
  CHECK(PC::parse_splat_binary({}) == nullptr);
  CHECK(PC::parse_splat_binary(std::string(31, 'x')) == nullptr);
  CHECK(PC::parse_splat_binary(std::string(33, 'x')) == nullptr);
  CHECK(PC::parse_splat_binary(std::string(1, 'x')) == nullptr);
  CHECK(PC::parse_splat_binary(std::string(65, 'x')) == nullptr);
}

// ================================================================= SpzCodec

namespace
{

// Column-major access into the canonical 62-float row.
constexpr uint32_t kRowFloats = 62;
constexpr uint32_t kPos = 0, kNormal = 3, kSHDC = 6, kSHRest = 9, kAlpha = 54,
                   kScale = 55, kRot = 58;

float row_at(const ossia::buffer_data* bd, uint32_t row, uint32_t f)
{
  return read_f32(bd, (size_t)row * kRowFloats * 4 + (size_t)f * 4);
}

spz::GaussianCloud two_point_cloud()
{
  spz::GaussianCloud c;
  c.numPoints = 2;
  c.shDegree = 0;
  c.positions = {0.5f, -1.25f, 2.f, -3.5f, 0.75f, -0.25f};
  c.scales = {-1.5f, -2.f, -2.5f, -3.f, -1.f, -2.f};
  // spz order: x, y, z, w (normalized).
  c.rotations = {0.f, 0.f, 0.f, 1.f, 0.5f, 0.5f, 0.5f, 0.5f};
  c.alphas = {0.5f, -0.5f};
  c.colors = {0.25f, -0.5f, 0.75f, -1.f, 0.f, 1.f};
  return c;
}

std::vector<uint8_t> pack_spz(const spz::GaussianCloud& c, uint32_t version)
{
  spz::PackOptions po;
  po.version = version;
  po.from = spz::CoordinateSystem::RDF; // codec unpacks to RDF -> identity
  po.sh1Bits = 8;                       // full-precision SH for tight margins
  po.shRestBits = 8;
  std::vector<uint8_t> out;
  REQUIRE(spz::saveSpz(c, po, &out));
  REQUIRE_FALSE(out.empty());
  return out;
}

// Unique SH fingerprint per (channel, coefficient) that sits exactly on
// an 8-bit quantization center (n/128), so the pack/unpack round trip is
// lossless and can be compared with a tight margin.
float sh_value(uint32_t ch, uint32_t coef)
{
  return (float)(coef * 3 + ch + 1) / 128.f;
}

// One point at origin with `coefs` SH coefficients per channel; the SH
// value for (channel ch, coefficient k) is a small unique fingerprint.
spz::GaussianCloud sh_cloud(int degree, uint32_t coefs)
{
  spz::GaussianCloud c;
  c.numPoints = 1;
  c.shDegree = degree;
  c.positions = {0.f, 0.f, 0.f};
  c.scales = {-2.f, -2.f, -2.f};
  c.rotations = {0.f, 0.f, 0.f, 1.f};
  c.alphas = {0.f};
  c.colors = {0.f, 0.f, 0.f};
  c.sh.resize((size_t)coefs * 3);
  for(uint32_t k = 0; k < coefs; ++k)
    for(uint32_t ch = 0; ch < 3; ++ch)
      c.sh[k * 3 + ch] = sh_value(ch, k);
  return c;
}

void check_two_point_cloud(const std::vector<uint8_t>& file)
{
  auto cloud = PC::parse_spz(
      std::string_view{(const char*)file.data(), file.size()});
  REQUIRE(cloud);
  CHECK(cloud->primitive_count == 2);
  CHECK(cloud->row_stride == kRowFloats * 4);
  CHECK(cloud->topology == ossia::primitive_topology::points);
  CHECK(cloud->format_id == "3dgs.classic");
  CHECK(cloud->struct_type_name == "Splat3DGS");
  CHECK(cloud->stable_id != 0);

  const auto* bd = raw_bytes(cloud);
  REQUIRE(bd->byte_size == 2 * kRowFloats * 4);

  const auto src = two_point_cloud();
  for(uint32_t i = 0; i < 2; ++i)
  {
    // Positions: 24-bit fixed point, 12 fractional bits -> ~2.4e-4 step.
    for(uint32_t k = 0; k < 3; ++k)
      CHECK(row_at(bd, i, kPos + k)
            == Approx(src.positions[i * 3 + k]).margin(0.001));

    // Normals: not in SPZ, zero-filled.
    for(uint32_t k = 0; k < 3; ++k)
      CHECK(row_at(bd, i, kNormal + k) == 0.f);

    // SH DC (colors): 8-bit with 0.15 color scale -> ~0.026 step.
    for(uint32_t k = 0; k < 3; ++k)
      CHECK(row_at(bd, i, kSHDC + k)
            == Approx(src.colors[i * 3 + k]).margin(0.05));

    // Degree 0: all 45 f_rest floats must stay zero.
    for(uint32_t k = 0; k < 45; ++k)
      CHECK(row_at(bd, i, kSHRest + k) == 0.f);

    // Alpha: sigmoid -> u8 -> inv-sigmoid.
    CHECK(row_at(bd, i, kAlpha) == Approx(src.alphas[i]).margin(0.05));

    // Scales (log-space): (s+10)*16 as u8 -> 1/16 step.
    for(uint32_t k = 0; k < 3; ++k)
      CHECK(row_at(bd, i, kScale + k)
            == Approx(src.scales[i * 3 + k]).margin(0.04));

    // Rotation: spz (x,y,z,w) -> canonical (w,x,y,z); u8 quantized.
    const float* q = &src.rotations[i * 4];
    CHECK(row_at(bd, i, kRot + 0) == Approx(q[3]).margin(0.02));
    CHECK(row_at(bd, i, kRot + 1) == Approx(q[0]).margin(0.02));
    CHECK(row_at(bd, i, kRot + 2) == Approx(q[1]).margin(0.02));
    CHECK(row_at(bd, i, kRot + 3) == Approx(q[2]).margin(0.02));
  }

  CHECK_FALSE(cloud->bounds.empty());
  CHECK(cloud->bounds.min[0] == Approx(-3.5f).margin(0.001));
  CHECK(cloud->bounds.min[1] == Approx(-1.25f).margin(0.001));
  CHECK(cloud->bounds.min[2] == Approx(-0.25f).margin(0.001));
  CHECK(cloud->bounds.max[0] == Approx(0.5f).margin(0.001));
  CHECK(cloud->bounds.max[1] == Approx(0.75f).margin(0.001));
  CHECK(cloud->bounds.max[2] == Approx(2.f).margin(0.001));
}

} // namespace

TEST_CASE(".spz: v2 round trip decodes into the canonical row layout", "[spz]")
{
  check_two_point_cloud(pack_spz(two_point_cloud(), 2));
}

TEST_CASE(".spz: v3 (smallest-three quaternions) round trip", "[spz]")
{
  check_two_point_cloud(pack_spz(two_point_cloud(), 3));
}

TEST_CASE(".spz: SH degree 1 coefficients are transposed channel-major", "[spz]")
{
  spz::GaussianCloud c;
  c.numPoints = 1;
  c.shDegree = 1;
  c.positions = {0.f, 0.f, 0.f};
  c.scales = {-2.f, -2.f, -2.f};
  c.rotations = {0.f, 0.f, 0.f, 1.f};
  c.alphas = {0.f};
  c.colors = {0.f, 0.f, 0.f};
  // 3 coefficients x 3 channels, channel-inner (rgb per coefficient).
  c.sh = {
      0.10f, 0.20f, 0.30f,  // coef 0: r g b
      -0.40f, 0.50f, -0.60f, // coef 1
      0.70f, -0.25f, 0.05f,  // coef 2
  };

  const auto file = pack_spz(c, 2);
  auto cloud = PC::parse_spz(
      std::string_view{(const char*)file.data(), file.size()});
  REQUIRE(cloud);
  CHECK(cloud->primitive_count == 1);

  const auto* bd = raw_bytes(cloud);
  // Canonical layout: R block @kSHRest+0, G block @+15, B block @+30.
  for(uint32_t coef = 0; coef < 3; ++coef)
  {
    CHECK(row_at(bd, 0, kSHRest + 0 * 15 + coef)
          == Approx(c.sh[coef * 3 + 0]).margin(0.02));
    CHECK(row_at(bd, 0, kSHRest + 1 * 15 + coef)
          == Approx(c.sh[coef * 3 + 1]).margin(0.02));
    CHECK(row_at(bd, 0, kSHRest + 2 * 15 + coef)
          == Approx(c.sh[coef * 3 + 2]).margin(0.02));
  }
  // Coefficients 3..14 of each channel stay zero.
  for(uint32_t ch = 0; ch < 3; ++ch)
    for(uint32_t coef = 3; coef < 15; ++coef)
      CHECK(row_at(bd, 0, kSHRest + ch * 15 + coef) == 0.f);
}

TEST_CASE(".spz: SH degree 3 fills all 15 coefficients per channel", "[spz]")
{
  const auto file = pack_spz(sh_cloud(3, 15), 2);
  auto cloud = PC::parse_spz(
      std::string_view{(const char*)file.data(), file.size()});
  REQUIRE(cloud);
  const auto* bd = raw_bytes(cloud);
  for(uint32_t ch = 0; ch < 3; ++ch)
    for(uint32_t coef = 0; coef < 15; ++coef)
      CHECK(row_at(bd, 0, kSHRest + ch * 15 + coef)
            == Approx(sh_value(ch, coef)).margin(0.005));
}

TEST_CASE(".spz: SH degree 4 is truncated to the 15-coefficient layout", "[spz]")
{
  // Degree 4 carries 24 coefficients per channel; the canonical
  // 3dgs.classic row only holds 15 -> coefficients 15..23 are dropped.
  const auto file = pack_spz(sh_cloud(4, 24), 2);
  auto cloud = PC::parse_spz(
      std::string_view{(const char*)file.data(), file.size()});
  REQUIRE(cloud);
  CHECK(cloud->row_stride == kRowFloats * 4); // layout unchanged
  const auto* bd = raw_bytes(cloud);
  for(uint32_t ch = 0; ch < 3; ++ch)
    for(uint32_t coef = 0; coef < 15; ++coef)
      CHECK(row_at(bd, 0, kSHRest + ch * 15 + coef)
            == Approx(sh_value(ch, coef)).margin(0.005));
  // Alpha/scale/rot fields directly after the SH block must be intact
  // (no overrun from the truncation).
  CHECK(row_at(bd, 0, kAlpha) == Approx(0.f).margin(0.05));
  CHECK(row_at(bd, 0, kScale) == Approx(-2.f).margin(0.04));
  CHECK(row_at(bd, 0, kRot) == Approx(1.f).margin(0.02));
}

TEST_CASE(".spz: RUB-tagged input is flipped to RDF", "[spz]")
{
  // Claim the data is already RUB at pack time (no conversion on save);
  // the codec always unpacks to RDF, so y and z must come out negated.
  auto c = two_point_cloud();
  spz::PackOptions po;
  po.version = 2;
  po.from = spz::CoordinateSystem::RUB;
  std::vector<uint8_t> file;
  REQUIRE(spz::saveSpz(c, po, &file));

  auto cloud = PC::parse_spz(
      std::string_view{(const char*)file.data(), file.size()});
  REQUIRE(cloud);
  const auto* bd = raw_bytes(cloud);
  CHECK(row_at(bd, 0, kPos + 0) == Approx(0.5f).margin(0.001));
  CHECK(row_at(bd, 0, kPos + 1) == Approx(1.25f).margin(0.001));  // -y
  CHECK(row_at(bd, 0, kPos + 2) == Approx(-2.f).margin(0.001));   // -z
}

TEST_CASE(".spz: corrupt / truncated / empty inputs fail gracefully", "[spz]")
{
  SECTION("empty")
  {
    CHECK(PC::parse_spz({}) == nullptr);
  }
  SECTION("garbage bytes")
  {
    const std::string junk(64, (char)0xAB);
    CHECK(PC::parse_spz(junk) == nullptr);
  }
  SECTION("truncated valid file")
  {
    const auto file = pack_spz(two_point_cloud(), 2);
    REQUIRE(file.size() > 8);
    CHECK(PC::parse_spz(std::string_view{(const char*)file.data(), file.size() / 2})
          == nullptr);
    CHECK(PC::parse_spz(std::string_view{(const char*)file.data(), 4}) == nullptr);
  }
  SECTION("single byte")
  {
    CHECK(PC::parse_spz(std::string_view{"x", 1}) == nullptr);
  }
}

// ============================================================ SceneFromCloud

TEST_CASE("SceneFromCloud: wraps a cloud into a one-node scene", "[scene]")
{
  auto cloud = make_cloud("3dgs.classic");
  auto state = PC::sceneStateFromCloud(cloud, "my_file.splat");
  REQUIRE(state);
  CHECK(state->version == 1);
  CHECK(state->dirty_index == 1);
  REQUIRE(state->roots);
  REQUIRE(state->roots->size() == 1);

  const auto& node = (*state->roots)[0];
  REQUIRE(node);
  CHECK(node->name == "my_file.splat");
  // Node id keyed on the raw_data pointer, and must be non-zero.
  CHECK(node->id.value == (uint64_t)(uintptr_t)cloud->raw_data.get());
  CHECK(node->id.value != 0);

  REQUIRE(node->children);
  REQUIRE(node->children->size() == 1);
  auto* pc
      = ossia::get_if<ossia::primitive_cloud_component_ptr>(&(*node->children)[0]);
  REQUIRE(pc);
  CHECK(pc->get() == cloud.get()); // same component, not a copy
}

TEST_CASE("SceneFromCloud: default label and null-raw_data key fallback",
          "[scene]")
{
  auto cloud = std::make_shared<ossia::primitive_cloud_component>();
  auto state
      = PC::sceneStateFromCloud(ossia::primitive_cloud_component_ptr{cloud});
  REQUIRE(state);
  const auto& node = (*state->roots)[0];
  CHECK(node->name == "primitive_cloud");
  // raw_data is null: id falls back to the component pointer.
  CHECK(node->id.value == (uint64_t)(uintptr_t)cloud.get());
  CHECK(node->id.value != 0);
}

TEST_CASE("SceneFromCloud: null cloud returns null", "[scene]")
{
  CHECK(PC::sceneStateFromCloud(nullptr) == nullptr);
  CHECK(PC::sceneStateFromCloud(nullptr, "label") == nullptr);
}

// ============================================================ FormatOverride

TEST_CASE("FormatOverride: null state and empty override shortcuts", "[override]")
{
  CHECK(PC::applyFormatOverride(nullptr, "foo") == nullptr);
  CHECK(PC::applyFormatOverride(nullptr, "") == nullptr);

  auto state = PC::sceneStateFromCloud(make_cloud(""), "n");
  auto same = PC::applyFormatOverride(state, "");
  CHECK(same.get() == state.get()); // verbatim, no clone, no version bump
  CHECK(same->version == state->version);
}

TEST_CASE("FormatOverride: rewrites the cloud format and bumps versions",
          "[override]")
{
  auto cloud = make_cloud("");
  auto state = PC::sceneStateFromCloud(cloud, "n");
  const auto old_version = state->version;
  const auto old_dirty = state->dirty_index;

  auto out = PC::applyFormatOverride(state, "custom.format");
  REQUIRE(out);
  CHECK(out.get() != state.get());
  CHECK(out->version == old_version + 1);
  CHECK(out->dirty_index == old_dirty + 1);

  // Original untouched.
  {
    auto* pc = ossia::get_if<ossia::primitive_cloud_component_ptr>(
        &(*(*state->roots)[0]->children)[0]);
    REQUIRE(pc);
    CHECK((*pc)->format_id.empty());
  }
  // Rewritten copy carries the override and shares the raw_data buffer.
  {
    REQUIRE(out->roots);
    REQUIRE(out->roots->size() == 1);
    const auto& node = (*out->roots)[0];
    CHECK(node.get() != (*state->roots)[0].get()); // fresh node
    auto* pc = ossia::get_if<ossia::primitive_cloud_component_ptr>(
        &(*node->children)[0]);
    REQUIRE(pc);
    CHECK(pc->get() != cloud.get()); // fresh component...
    CHECK((*pc)->format_id == "custom.format");
    CHECK((*pc)->raw_data.get() == cloud->raw_data.get()); // ...shared payload
    CHECK((*pc)->primitive_count == cloud->primitive_count);
    CHECK((*pc)->stable_id == cloud->stable_id);
  }
}

TEST_CASE("FormatOverride: matching format keeps node identity", "[override]")
{
  auto state = PC::sceneStateFromCloud(make_cloud("already.set"), "n");
  auto out = PC::applyFormatOverride(state, "already.set");
  REQUIRE(out);
  CHECK(out.get() != state.get()); // state itself is still cloned...
  CHECK(out->version == state->version + 1);
  // ...but the untouched tree keeps its identity for fingerprinting.
  CHECK(out->roots.get() == state->roots.get());
  CHECK((*out->roots)[0].get() == (*state->roots)[0].get());
}

TEST_CASE("FormatOverride: rewrites nested nodes, preserves siblings",
          "[override]")
{
  // root
  //  +- scene_transform            (must keep identity)
  //  +- inner scene_node
  //      +- cloud (format "")     (must be rewritten)
  auto cloud = make_cloud("");

  auto inner_children = std::make_shared<std::vector<ossia::scene_payload>>();
  inner_children->push_back(ossia::primitive_cloud_component_ptr{cloud});
  auto inner = std::make_shared<ossia::scene_node>();
  inner->id.value = 2;
  inner->name = "inner";
  inner->children = inner_children;

  auto root_children = std::make_shared<std::vector<ossia::scene_payload>>();
  root_children->push_back(ossia::scene_transform{});
  root_children->push_back(ossia::scene_node_ptr{inner});
  auto root = std::make_shared<ossia::scene_node>();
  root->id.value = 1;
  root->name = "root";
  root->children = root_children;

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(root);
  auto state = std::make_shared<ossia::scene_state>();
  state->roots = roots;
  state->version = 7;
  state->dirty_index = 3;

  auto out = PC::applyFormatOverride(state, "fmt.x");
  REQUIRE(out);
  CHECK(out->version == 8);
  CHECK(out->dirty_index == 4);
  REQUIRE(out->roots);
  REQUIRE(out->roots->size() == 1);

  const auto& new_root = (*out->roots)[0];
  CHECK(new_root.get() != root.get()); // rewritten because a descendant changed
  REQUIRE(new_root->children);
  REQUIRE(new_root->children->size() == 2);

  // Sibling transform payload survives untouched.
  CHECK(ossia::get_if<ossia::scene_transform>(&(*new_root->children)[0]));

  auto* new_inner = ossia::get_if<ossia::scene_node_ptr>(&(*new_root->children)[1]);
  REQUIRE(new_inner);
  CHECK(new_inner->get() != inner.get()); // fresh nested node
  auto* pc = ossia::get_if<ossia::primitive_cloud_component_ptr>(
      &(*(*new_inner)->children)[0]);
  REQUIRE(pc);
  CHECK((*pc)->format_id == "fmt.x");
  CHECK((*pc)->raw_data.get() == cloud->raw_data.get());

  // Original tree untouched.
  CHECK(cloud->format_id.empty());
  CHECK((*state->roots)[0].get() == root.get());
}

TEST_CASE("FormatOverride: childless nodes and null clouds pass through",
          "[override]")
{
  auto empty_node = std::make_shared<ossia::scene_node>();
  empty_node->id.value = 5;

  auto null_cloud_children = std::make_shared<std::vector<ossia::scene_payload>>();
  null_cloud_children->push_back(ossia::primitive_cloud_component_ptr{});
  auto null_cloud_node = std::make_shared<ossia::scene_node>();
  null_cloud_node->id.value = 6;
  null_cloud_node->children = null_cloud_children;

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(empty_node);
  roots->push_back(null_cloud_node);
  roots->push_back(nullptr); // null root must not crash
  auto state = std::make_shared<ossia::scene_state>();
  state->roots = roots;

  auto out = PC::applyFormatOverride(state, "fmt.y");
  REQUIRE(out);
  // Nothing rewritable anywhere: tree identity preserved wholesale.
  CHECK(out->roots.get() == state->roots.get());
  CHECK(out->version == state->version + 1);
}
