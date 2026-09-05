// Source-text guards for the four fulldome fisheye projections and the Camera
// combo that selects them.
//
// The projections are constant GLSL strings compiled into the ModelDisplay
// pipeline, one per camera_mode, and the combo entries are plain integers in
// the process model — neither is reachable without a full RenderList, so the
// honest GPU-free guard reads the SHIPPED engine source, exactly as
// ShaderStrings.cpp does for findings #5 / #6.
//
// The property being pinned is that each law's denominator is its own
// numerator evaluated at the half-FOV:
//   equidistant    r = theta                / (fov/2)
//   equisolid      r = sin(theta * 0.5)     / sin(fov/4)
//   stereographic  r = tan(theta * 0.5)     / tan(fov/4)
//   orthographic   r = sin(theta)           / sin(fov/2)
// The realistic regression is a fov/2-vs-fov/4 mix-up between the half-angle
// families (equisolid, stereographic) and the full-angle ones, so each case
// asserts both the right pairing and the absence of the wrong one.

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <string>

namespace
{
std::string slurp(const std::string& path)
{
  std::ifstream f(path, std::ios::binary);
  REQUIRE(f.good());
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

std::string modelDisplayNodeSource()
{
  return slurp(
      std::string(THREEDIM_SRC_DIR) + "/ModelDisplay/ModelDisplayNode.cpp");
}

std::string modelDisplayProcessSource()
{
  return slurp(std::string(THREEDIM_SRC_DIR) + "/ModelDisplay/Process.cpp");
}

bool has(const std::string& hay, const char* needle)
{
  return hay.find(needle) != std::string::npos;
}

// Index of `needle` in `hay`, or npos. Used for the ordering assertions.
std::size_t at(const std::string& hay, const char* needle)
{
  return hay.find(needle);
}
} // namespace

TEST_CASE(
    "all four fulldome projection snippets exist", "[threedim][shader][fisheye]")
{
  const std::string src = modelDisplayNodeSource();

  CHECK(has(src, "vtx_projection_fulldome_equidistant"));
  CHECK(has(src, "vtx_projection_fulldome_equisolid"));
  CHECK(has(src, "vtx_projection_fulldome_stereographic"));
  CHECK(has(src, "vtx_projection_fulldome_orthographic"));
}

TEST_CASE(
    "each fisheye law normalises by its own numerator at the half-FOV",
    "[threedim][shader][fisheye]")
{
  const std::string src = modelDisplayNodeSource();

  SECTION("equidistant: theta / (fov/2)")
  {
    CHECK(has(src, "float half_fov_rad = max(radians(camera.fov * 0.5), 1e-6);"));
    CHECK(has(src, "float r_ndc = theta / half_fov_rad;"));
    CHECK_FALSE(has(src, "float r_ndc = theta / quarter_fov_rad;"));
  }

  SECTION("equisolid: sin(theta/2) / sin(fov/4)")
  {
    CHECK(has(
        src, "float quarter_fov_rad = max(radians(camera.fov * 0.25), 1e-6);"));
    CHECK(has(src, "float r_ndc = sin(theta * 0.5) / sin(quarter_fov_rad);"));
    CHECK_FALSE(has(src, "float r_ndc = sin(theta * 0.5) / sin(half_fov_rad);"));
  }

  SECTION("stereographic: tan(theta/2) / tan(fov/4)")
  {
    CHECK(has(src, "float r_ndc = tan(theta * 0.5) / tan(quarter_fov_rad);"));
    CHECK_FALSE(has(src, "float r_ndc = tan(theta * 0.5) / tan(half_fov_rad);"));
  }

  SECTION("orthographic: sin(theta) / sin(fov/2)")
  {
    CHECK(has(src, "float r_ndc = sin(theta) / sin(half_fov_rad);"));
    CHECK_FALSE(has(src, "float r_ndc = sin(theta) / sin(quarter_fov_rad);"));
  }

  SECTION("the four laws are four distinct expressions")
  {
    const char* laws[]{
        "float r_ndc = theta / half_fov_rad;",
        "float r_ndc = sin(theta * 0.5) / sin(quarter_fov_rad);",
        "float r_ndc = tan(theta * 0.5) / tan(quarter_fov_rad);",
        "float r_ndc = sin(theta) / sin(half_fov_rad);"};
    for(const char* a : laws)
      for(const char* b : laws)
        if(a != b)
          CHECK(std::string(a) != std::string(b));
  }
}

TEST_CASE(
    "equidistant keeps the pre-7b2704dead radial mapping",
    "[threedim][shader][fisheye]")
{
  const std::string src = modelDisplayNodeSource();

  CHECK(has(src, "float r_ndc = theta / half_fov_rad;"));
  CHECK_FALSE(has(src, "proj_ratio"));
}

TEST_CASE(
    "projections[] lists the five camera modes in UI order",
    "[threedim][shader][fisheye]")
{
  const std::string src = modelDisplayNodeSource();

  CHECK(has(src, "static constexpr int CAMERA_MODE_COUNT = 5;"));

  const std::size_t table = at(src, "const char* projections[CAMERA_MODE_COUNT]");
  REQUIRE(table != std::string::npos);
  const std::string list = src.substr(table, 400);

  const std::size_t persp = at(list, "vtx_projection_perspective");
  const std::size_t equid = at(list, "vtx_projection_fulldome_equidistant");
  const std::size_t equis = at(list, "vtx_projection_fulldome_equisolid");
  const std::size_t stereo = at(list, "vtx_projection_fulldome_stereographic");
  const std::size_t ortho = at(list, "vtx_projection_fulldome_orthographic");

  REQUIRE(persp != std::string::npos);
  REQUIRE(equid != std::string::npos);
  REQUIRE(equis != std::string::npos);
  REQUIRE(stereo != std::string::npos);
  REQUIRE(ortho != std::string::npos);

  CHECK(persp < equid);
  CHECK(equid < equis);
  CHECK(equis < stereo);
  CHECK(stereo < ortho);
}

TEST_CASE(
    "the Camera combo indices agree with projections[]",
    "[threedim][shader][fisheye][combo]")
{
  const std::string src = modelDisplayProcessSource();

  CHECK(has(src, "{\"Perspective\", 0}"));
  // Index 1 must stay equidistant: documents saved when "Fulldome (1-pass)"
  // was the only dome mode store a 1 and must keep the picture they had.
  CHECK(has(src, "{\"Fulldome (equidistant)\", 1}"));
  CHECK(has(src, "{\"Fulldome (equisolid)\", 2}"));
  CHECK(has(src, "{\"Fulldome (stereographic)\", 3}"));
  CHECK(has(src, "{\"Fulldome (orthographic)\", 4}"));

  CHECK_FALSE(has(src, "Fulldome (1-pass)"));
}
