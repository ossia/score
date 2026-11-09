// REPEAT groups run their passes the count of times in a row, each dispatch
// reading its own PASSINDEX and the iteration of its innermost group as
// ITERATIONINDEX: a count from a control, a count of 0, a nested group, and a
// count changed between frames. csf-repeat-recurrence.cs folds every
// iteration into an order-dependent recurrence and logs what each dispatch
// read; a dispatch reading another dispatch's iteration changes both.
//
//   SCORE_TEST_API=vulkan ctest -R gfx_csf_repeat
#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <vector>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
struct Expected
{
  std::vector<int> log;
  int acc{};
};

// What the shader computes with `iterations` repetitions of its first group.
Expected reference(int iterations)
{
  Expected e;
  for(int it = 0; it < iterations; it++)
  {
    e.acc = (e.acc * 3 + it + 1) % 251;
    e.acc = (e.acc * 5 + 2) % 251;
    e.log.push_back(it * 16 + 2);
  }
  for(int outer = 0; outer < 2; outer++)
    for(int it = 0; it < 2; it++)
      e.log.push_back(100 + it * 10 + 4);
  return e;
}

struct Shot
{
  bool skipped{};
  std::string error;
  std::vector<ReadbackImage> frames;
};

// Renders with the control at each of `counts` in turn, unset first.
Shot run(score::gfx::GraphicsApi api, const std::vector<int>& counts)
{
  Shot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int csf = p.addCsf(corpus("csf-repeat-recurrence.cs"));
    const int sink = p.addSink({16, 2});
    if(csf < 0)
    {
      r.error = "build failed: " + p.error();
      return;
    }
    p.wire(p.imageOut(csf, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    for(std::size_t k = 0; k < counts.size(); k++)
    {
      if(k > 0)
        setControl(*p.isf(csf), 0, ossia::value{counts[k]});
      p.render(3);
      r.frames.push_back(p.readback(sink));
    }
  });
  return r;
}

void check(const ReadbackImage& img, int iterations)
{
  INFO("iterations " << iterations);
  REQUIRE(img.valid());
  REQUIRE(img.width == 16);
  REQUIRE(img.height == 2);
  const Expected e = reference(iterations);
  for(int x = 0; x < 16; x++)
  {
    const int want = x < (int)e.log.size() ? e.log[x] : 0;
    INFO("log " << x);
    CHECK(int(img.at(x, 0)[0]) == want);
  }
  CHECK(int(img.at(0, 1)[0]) == e.acc);
  CHECK(int(img.at(1, 1)[0]) == (int)e.log.size());
}
}

TEST_CASE(
    "REPEAT dispatches read their own PASSINDEX and ITERATIONINDEX",
    "[gfx][csf][repeat]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  // The control's default, then 0, then more: the schedule follows.
  const Shot r = run(api, {3, 0, 5});
  if(r.skipped)
    SKIP("backend unavailable");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.frames.size() == 3);
  check(r.frames[0], 3);
  check(r.frames[1], 0);
  check(r.frames[2], 5);
}
