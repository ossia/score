// An optional read_only CSF attribute with a DEFAULT reads that value where
// the upstream lacks the attribute, typed as declared (float, int, uint,
// vectors, per-instance), while one the upstream provides reads the
// upstream's values even when they are zeros. Switching the input between an
// upstream with the attribute and one without follows the change both ways.
//
// csf-default-probe.cs paints red = weight, green = offset.y and blue = 1 when
// the int, uvec2, per-instance vec4 and DEFAULT-less attributes read their
// fallbacks; black when the elements disagree.
//
//   SCORE_TEST_API=vulkan ctest -R gfx_csf_attribute_default
#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
struct Probe
{
  bool skipped = false;
  std::string error;
  std::vector<std::array<int, 3>> centers;
};

std::array<int, 3> center(const ReadbackImage& img)
{
  const auto c = img.center();
  return {int(c[0]), int(c[1]), int(c[2])};
}

// Renders probe <- producers[0], then rewires the probe to each further
// producer in turn, reading the probe's image after each.
Probe run(score::gfx::GraphicsApi api, std::vector<const char*> producers)
{
  Probe r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    std::vector<int> prod;
    for(auto* f : producers)
      prod.push_back(p.addCsf(corpus(f)));
    const int probe = p.addCsf(corpus("csf-default-probe.cs"));
    const int sink = p.addSink({16, 16});
    if(probe < 0 || std::find(prod.begin(), prod.end(), -1) != prod.end())
    {
      r.error = "build failed: " + p.error();
      return;
    }
    p.wire(p.geometryOut(prod[0], 0), p.geometryIn(probe, 0));
    p.wire(p.imageOut(probe, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    r.centers.push_back(center(p.readback(sink)));
    for(std::size_t k = 1; k < prod.size(); k++)
    {
      p.removeEdgeIncremental(p.geometryOut(prod[k - 1], 0), p.geometryIn(probe, 0));
      p.addEdgeIncremental(p.geometryOut(prod[k], 0), p.geometryIn(probe, 0));
      p.render(3);
      r.centers.push_back(center(p.readback(sink)));
    }
  });
  return r;
}

bool close_to(const std::array<int, 3>& a, std::array<int, 3> b)
{
  for(int c = 0; c < 3; c++)
    if(std::abs(a[c] - b[c]) > 3)
      return false;
  return true;
}

// weight 1, offset.y 0.5, every other fallback as declared.
constexpr std::array<int, 3> defaults{255, 128, 255};
// The producer's weight 0 and offset.y 0.2; the rest still fall back.
constexpr std::array<int, 3> provided{0, 51, 255};
}

TEST_CASE(
    "an absent attribute reads its typed DEFAULT", "[gfx][csf][geometry][default]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const Probe r = run(api, {"csf-default-producer-bare.cs"});
  if(r.skipped)
    SKIP("backend unavailable");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.centers.size() == 1);
  const auto c = r.centers[0];
  INFO("center " << c[0] << "," << c[1] << "," << c[2]);
  CHECK(close_to(c, defaults));
}

TEST_CASE(
    "a present attribute wins over its DEFAULT, zeros included",
    "[gfx][csf][geometry][default]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const Probe r = run(api, {"csf-default-producer-full.cs"});
  if(r.skipped)
    SKIP("backend unavailable");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.centers.size() == 1);
  const auto c = r.centers[0];
  INFO("center " << c[0] << "," << c[1] << "," << c[2]);
  CHECK(close_to(c, provided));
}

TEST_CASE(
    "an attribute that disappears from the upstream reads its DEFAULT again",
    "[gfx][csf][geometry][default]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const Probe r = run(
      api, {"csf-default-producer-full.cs", "csf-default-producer-bare.cs",
            "csf-default-producer-full.cs"});
  if(r.skipped)
    SKIP("backend unavailable");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.centers.size() == 3);
  for(const auto& c : r.centers)
    UNSCOPED_INFO("center " << c[0] << "," << c[1] << "," << c[2]);
  CHECK(close_to(r.centers[0], provided));
  CHECK(close_to(r.centers[1], defaults));
  CHECK(close_to(r.centers[2], provided));
}
