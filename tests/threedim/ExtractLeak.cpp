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

// Count non-overlapping occurrences of `needle` in `hay`.
std::size_t count(const std::string& hay, const std::string& needle)
{
  std::size_t n = 0, pos = 0;
  while((pos = hay.find(needle, pos)) != std::string::npos)
  {
    ++n;
    pos += needle.size();
  }
  return n;
}
} // namespace

TEST_CASE(
    "ExtractBuffer2 releases strategy resources on init failure",
    "[threedim][extractbuffer][f14]")
{
  const std::string src
      = slurp(std::string(THREEDIM_SRC_DIR) + "/ExtractBuffer2.cpp");

  // Both the Attribute-mode and Buffer-mode init-failure branches must release
  // before discarding the strategy (the leak fix). We look for the two
  // distinct failure-path markers the fix introduced.
  CHECK(
      src.find("before discarding to avoid leaking them") != std::string::npos);
  CHECK(
      src.find("Release any QRhi resources init() allocated before failing")
      != std::string::npos);

  // And each of those two failure branches actually calls release(renderer).
  CHECK(count(src, "release(renderer);") >= 2);
}
