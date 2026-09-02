// KmsOutputNode is a sink: the graph renders into its image input and the node
// scans that texture out. It was added without ever creating that port, so the
// first thing to touch input[0] -- connecting anything to it -- indexed an empty
// vector.
//
// It compiles and links either way, and every accessor that does not go through
// input[] behaves identically, so nothing short of building a graph around it
// showed the difference. Asserting the port directly is the cheap form of that:
// no device, no DRM master, no QRhi.

#include <Gfx/Graph/KmsOutputNode.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("the KMS output has an image input to scan out")
{
  // Default settings on purpose: nothing here opens a card. The port is created
  // by the constructor, which is where the omission was.
  score::gfx::KmsOutputNode n{score::gfx::KmsOutputSettings{}};

  // Exactly one, not "at least one": the node only ever scans out input[0], so a
  // second image input would be a second thing the graph could render into with
  // nothing reading it.
  REQUIRE(n.input.size() == 1u);
  REQUIRE(n.input[0] != nullptr);

  // Types::Image is what makes the port connectable to a texture edge at all --
  // a port of any other type would exist and still refuse every cable the node
  // exists to accept.
  CHECK(n.input[0]->type == score::gfx::Types::Image);
  CHECK(n.input[0]->node == &n);
  CHECK(n.input[0]->edges.empty());
}

TEST_CASE("the KMS output is a sink, so it produces nothing")
{
  score::gfx::KmsOutputNode n{score::gfx::KmsOutputSettings{}};
  CHECK(n.output.empty());
}
