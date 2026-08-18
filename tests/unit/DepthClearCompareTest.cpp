// UNIT — the depth clear value must be usable by the depth compare a shader
// declares (#172).
//
// PIPELINE_STATE's DEPTH_COMPARE accepts both directions. The clear value the
// draw passes begin with is fixed at 0.0 (RenderedRawRasterPipelineNode.cpp and
// RenderList.cpp, both through depthClearForCompare()), which is what the
// project-wide reverse-Z convention wants — reverse-Z pairs D32F + GREATER +
// clear 0.0, see CameraMath.hpp.
//
// Depth after the viewport transform is in [0, 1], so a clear value only admits
// fragments on one side of itself. Cleared to 0.0, a `less` compare admits
// nothing at all and a `less_equal` compare admits only the single plane at
// exactly 0.0: the first draw into the target is entirely rejected and the
// frame stays at the clear colour, with no diagnostic. tests-scene's
// ps-depth-test.fs declares exactly that combination, and ShaderSweepScene
// records "nothing drawn" as an unasserted observation.
//
// EXPECTED RED for the `less` rows until the clear is derived from the declared
// compare.

#include <Gfx/Graph/PipelineStateHelpers.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using namespace score::gfx;

namespace
{
constexpr int kSamples = 1000;

//! How many depths sampled over [0, 1] the depth test admits against `clear`.
//! A clear paired with its compare admits the whole range bar the clear plane
//! itself; a clear paired with the opposite compare admits ~nothing.
int depthsAdmitted(QRhiGraphicsPipeline::CompareOp compare, float clear)
{
  int n = 0;
  for(int i = 0; i <= kSamples; i++)
  {
    const float d = float(i) / float(kSamples);
    bool pass = false;
    switch(compare)
    {
      case QRhiGraphicsPipeline::Never:          pass = false;      break;
      case QRhiGraphicsPipeline::Always:         pass = true;       break;
      case QRhiGraphicsPipeline::Less:           pass = d < clear;  break;
      case QRhiGraphicsPipeline::LessOrEqual:    pass = d <= clear; break;
      case QRhiGraphicsPipeline::Greater:        pass = d > clear;  break;
      case QRhiGraphicsPipeline::GreaterOrEqual: pass = d >= clear; break;
      case QRhiGraphicsPipeline::Equal:          pass = d == clear; break;
      case QRhiGraphicsPipeline::NotEqual:       pass = d != clear; break;
    }
    n += pass ? 1 : 0;
  }
  return n;
}

//! The property a usable (clear, compare) pairing has: every depth in the
//! range except at most the clear plane itself draws into a freshly cleared
//! target.
bool clearIsUsableBy(QRhiGraphicsPipeline::CompareOp compare)
{
  return depthsAdmitted(compare, depthClearForCompare(compare)) >= kSamples;
}
}

TEST_CASE("toCompareOp maps the DEPTH_COMPARE vocabulary", "[pipeline_state][depth]")
{
  CHECK(toCompareOp("never") == QRhiGraphicsPipeline::Never);
  CHECK(toCompareOp("less") == QRhiGraphicsPipeline::Less);
  CHECK(toCompareOp("LESS") == QRhiGraphicsPipeline::Less);
  CHECK(toCompareOp("equal") == QRhiGraphicsPipeline::Equal);
  CHECK(toCompareOp("less_equal") == QRhiGraphicsPipeline::LessOrEqual);
  CHECK(toCompareOp("lequal") == QRhiGraphicsPipeline::LessOrEqual);
  CHECK(toCompareOp("lessOrEqual") == QRhiGraphicsPipeline::LessOrEqual);
  CHECK(toCompareOp("greater") == QRhiGraphicsPipeline::Greater);
  CHECK(toCompareOp("greater_equal") == QRhiGraphicsPipeline::GreaterOrEqual);
  CHECK(toCompareOp("not_equal") == QRhiGraphicsPipeline::NotEqual);
  CHECK(toCompareOp("always") == QRhiGraphicsPipeline::Always);
  CHECK(toCompareOp("nonsense") == QRhiGraphicsPipeline::Less);
}

TEST_CASE("depthsAdmitted describes the depth test the way the pipeline does", "[pipeline_state][depth]")
{
  CHECK(depthsAdmitted(QRhiGraphicsPipeline::Greater, 0.f) == kSamples);
  CHECK(depthsAdmitted(QRhiGraphicsPipeline::Less, 0.f) == 0);
  CHECK(depthsAdmitted(QRhiGraphicsPipeline::LessOrEqual, 0.f) == 1);
  CHECK(depthsAdmitted(QRhiGraphicsPipeline::Less, 1.f) == kSamples);
  CHECK(depthsAdmitted(QRhiGraphicsPipeline::Never, 0.f) == 0);
  CHECK(depthsAdmitted(QRhiGraphicsPipeline::Always, 0.f) == kSamples + 1);
}

TEST_CASE("the reverse-Z compares are usable against the depth clear", "[pipeline_state][depth]")
{
  CHECK(clearIsUsableBy(toCompareOp("greater")));
  CHECK(clearIsUsableBy(toCompareOp("greater_equal")));
  CHECK(clearIsUsableBy(toCompareOp("always")));
  CHECK_FALSE(clearIsUsableBy(toCompareOp("never")));
}

TEST_CASE("a shader declaring a less compare gets a clear it can pass", "[pipeline_state][depth][issue172]")
{
  for(std::string_view s : {"less", "less_equal", "lequal"})
  {
    INFO("DEPTH_COMPARE: " << s);
    CHECK(clearIsUsableBy(toCompareOp(s)));
  }
}

TEST_CASE("the depth clear differs between the two compare directions", "[pipeline_state][depth][issue172]")
{
  CHECK(
      depthClearForCompare(QRhiGraphicsPipeline::Less)
      != depthClearForCompare(QRhiGraphicsPipeline::Greater));
}
