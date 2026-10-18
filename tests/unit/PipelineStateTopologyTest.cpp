// PIPELINE_STATE TOPOLOGY names. The name matcher ignores case, '_', '-' and
// spaces, but stripped them only from the shader's string: a vocabulary entry
// spelled with an underscore ("triangle_strip") never matched, and every strip,
// fan and line list fell back to triangles (shader board R13).
#include <Gfx/Graph/PipelineStateHelpers.hpp>

#include <catch2/catch_test_macros.hpp>

using score::gfx::toTopology;
using T = QRhiGraphicsPipeline::Topology;

TEST_CASE("toTopology maps every TOPOLOGY spelling", "[pipeline_state][topology][R13]")
{
  CHECK(toTopology("triangles") == T::Triangles);
  CHECK(toTopology("triangle_list") == T::Triangles);
  CHECK(toTopology("triangle_strip") == T::TriangleStrip);
  CHECK(toTopology("TRIANGLE_STRIP") == T::TriangleStrip);
  CHECK(toTopology("trianglestrip") == T::TriangleStrip);
  CHECK(toTopology("triangle-strip") == T::TriangleStrip);
  CHECK(toTopology("triangle_fan") == T::TriangleFan);
  CHECK(toTopology("lines") == T::Lines);
  CHECK(toTopology("line_list") == T::Lines);
  CHECK(toTopology("line_strip") == T::LineStrip);
  CHECK(toTopology("points") == T::Points);
}
