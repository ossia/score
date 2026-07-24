// L3 regression guards — split/threedim shader-string fixes.
//
// Findings #5 and #6 from REVIEW-THREEDIM.md are pure GLSL-string bugs in the
// ModelDisplay fragment shaders (ModelDisplayNode.cpp). Both are constant
// shader strings emitted verbatim into the pipeline, so the honest, GPU-free
// regression guard is to read the SHIPPED engine source and assert the shader
// text is the corrected form (and NOT the buggy form). Reverting the engine
// fix (git checkout of the pre-fix ModelDisplayNode.cpp) flips these RED.
//
//   #5 Triplanar fragment shader double-weighted the X projection and dropped
//      the Y projection: the middle blend term must use `yaxis`, not `xaxis`.
//   #6 Spherical (equirectangular) projection scaled longitude by the wrong
//      value due to GLSL operator precedence: the denominator must be
//      parenthesised `1. / (2. * PI)`, not `1. / 2. * PI` (== 0.5*PI).
//
// THREEDIM_SRC_DIR is injected by CMake as an absolute path so the test is
// self-contained and does not depend on any working directory.

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

std::string modelDisplaySource()
{
  return slurp(
      std::string(THREEDIM_SRC_DIR) + "/ModelDisplay/ModelDisplayNode.cpp");
}
} // namespace

TEST_CASE("triplanar shader combines the Y projection", "[threedim][shader][f5]")
{
  const std::string src = modelDisplaySource();

  // The corrected blend uses each of the three planar samples once, with the
  // MIDDLE term weighted by blending.y coming from `yaxis`.
  CHECK(src.find("yaxis * blending.y") != std::string::npos);

  // The bug double-weighted X: `xaxis * blending.x + xaxis * blending.y`.
  // That exact double-`xaxis` term must be gone.
  CHECK(src.find("xaxis * blending.y") == std::string::npos);

  // Sanity: the three axis samples and the full blend are all present so we
  // are matching the right shader (guards against the file being gutted).
  CHECK(src.find("vec4 yaxis = texture(") != std::string::npos);
  CHECK(
      src.find("xaxis * blending.x + yaxis * blending.y + zaxis * blending.z")
      != std::string::npos);
}

TEST_CASE(
    "equirectangular longitude scale is parenthesised", "[threedim][shader][f6]")
{
  const std::string src = modelDisplaySource();

  // Corrected: 1/(2*PI) with an explicit parenthesised denominator.
  CHECK(src.find("1. / (2. * 3.14159") != std::string::npos);

  // Buggy: `1. / 2. * 3.14159...` evaluates left-to-right to 0.5*PI. That
  // unparenthesised form must not appear.
  CHECK(src.find("1. / 2. * 3.14159") == std::string::npos);
}
