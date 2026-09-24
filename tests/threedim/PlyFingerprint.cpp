// PlyParser::detect_format_id tags a PLY by its columns. A 2DGS file (scale_0,
// scale_1, no scale_2) and a Mip-Splatting / GOF file (the 3DGS columns plus
// filter_3D) must not be tagged 3dgs.classic / Splat3DGS, whose row is 62
// floats: they get 2dgs.surfel / Splat2DGS and 3dgs.mip / SplatMip, the
// struct names the splat-formats bundles declare.

#include <Threedim/PrimitiveCloud/PlyParser.hpp>

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <string>
#include <vector>

namespace PC = Threedim::PrimitiveCloud;

namespace
{
std::vector<std::string> inria_columns(int scales, bool filter)
{
  std::vector<std::string> c{"x", "y", "z", "nx", "ny", "nz", "f_dc_0", "f_dc_1", "f_dc_2"};
  for(int i = 0; i < 45; ++i)
    c.push_back("f_rest_" + std::to_string(i));
  c.push_back("opacity");
  for(int i = 0; i < scales; ++i)
    c.push_back("scale_" + std::to_string(i));
  for(int i = 0; i < 4; ++i)
    c.push_back("rot_" + std::to_string(i));
  if(filter)
    c.push_back("filter_3D");
  return c;
}

std::string write_ply(QTemporaryDir& dir, const char* name, const std::vector<std::string>& cols)
{
  std::string s = "ply\nformat ascii 1.0\nelement vertex 3\n";
  for(auto& c : cols)
    s += "property float " + c + "\n";
  s += "end_header\n";
  for(int r = 0; r < 3; ++r)
  {
    for(std::size_t i = 0; i < cols.size(); ++i)
      s += std::to_string(float(r + i) * 0.5f) + (i + 1 == cols.size() ? "\n" : " ");
  }
  const auto path = dir.filePath(QString::fromUtf8(name)).toStdString();
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  f << s;
  return path;
}

struct Case
{
  const char* file;
  int scales;
  bool filter;
  const char* format_id;
  const char* struct_name;
  int floats;
};
}

TEST_CASE("PLY fingerprint distinguishes 3DGS, Mip/GOF and 2DGS rows", "[threedim][ply]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const Case cases[] = {
      {"classic.ply", 3, false, "3dgs.classic", "Splat3DGS", 62},
      {"mip.ply", 3, true, "3dgs.mip", "SplatMip", 63},
      {"surfel.ply", 2, false, "2dgs.surfel", "Splat2DGS", 61},
  };
  for(const auto& c : cases)
  {
    INFO(c.file);
    const auto cols = inria_columns(c.scales, c.filter);
    REQUIRE((int)cols.size() == c.floats);
    auto cloud = PC::parse_ply(write_ply(dir, c.file, cols));
    REQUIRE(cloud);
    CHECK(cloud->primitive_count == 3);
    CHECK(cloud->row_stride == uint32_t(c.floats * 4));
    CHECK(cloud->format_id == c.format_id);
    CHECK(cloud->struct_type_name == c.struct_name);
  }
}

TEST_CASE("PLY fingerprint leaves a 2DGS row with filter_3D untagged", "[threedim][ply]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  auto cloud = PC::parse_ply(write_ply(dir, "odd.ply", inria_columns(2, true)));
  REQUIRE(cloud);
  CHECK(cloud->format_id.empty());
  CHECK(cloud->struct_type_name.empty());
}
