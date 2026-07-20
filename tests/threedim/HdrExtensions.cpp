// The extension-filter uses the glob syntax `*.<ext>`. The explanatory
// comments in the header mention ".hdr/.exr" WITHOUT the leading asterisk, so
// matching on the `*.hdr` / `*.exr` glob tokens cleanly targets the advertised
// filter and not the prose.

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
} // namespace

TEST_CASE(
    "CubemapLoader no longer advertises .hdr/.exr", "[threedim][cubemap][f12]")
{
  const std::string src
      = slurp(std::string(THREEDIM_SRC_DIR) + "/CubemapLoader.hpp");

  // The advertised LDR filter is still present (guards against a gutted file).
  CHECK(src.find("halp_meta(extensions,") != std::string::npos);
  CHECK(src.find("*.png") != std::string::npos);

  // The float formats must NOT be advertised as selectable extensions.
  CHECK(src.find("*.hdr") == std::string::npos);
  CHECK(src.find("*.exr") == std::string::npos);
}
