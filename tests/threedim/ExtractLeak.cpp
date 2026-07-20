// L3 regression guard — split/threedim finding #14 (ExtractBuffer2 partial-init
// leak).
//
// HONEST SCOPE / DOCUMENTED GAP.
//
// The bug: ExtractBuffer2::initStrategy() emplaced a strategy, called its
// init(), and on failure did `m_strategy = std::monostate{}`. The strategy
// classes own raw QRhi resources (m_outputBuffer / m_uniformBuffer / m_srb /
// m_pipeline), have NO destructor, and free those resources ONLY in release().
// Assigning monostate destroys the strategy without calling release(), leaking
// every QRhi object allocated before the failure. The fix calls release()
// before discarding on BOTH failure paths (Attribute and Buffer modes).
//
// A true runtime leak test would require a QRhi backend on which a strategy's
// output-buffer create() succeeds but its compute-pipeline create() fails
// (compute unsupported / transient OOM) — not something reproducible or
// deterministic in a headless CI unit test. So, per the review's allowance
// ("a focused assertion on the failure path or an HONEST documented gap"),
// this guard asserts the shipped source's failure paths now route through
// release() rather than silently dropping the strategy. Reverting the fix
// flips it RED.

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
