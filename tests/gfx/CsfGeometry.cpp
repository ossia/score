// =============================================================================
// L3 — CSF GEOMETRY / STORAGE-BUFFER producers.
//
// These CSFs write their result into a vertex/storage SSBO exposed through a
// Types::Geometry / Types::Buffer OUTPUT port (not a texture). Reading that back
// is NOT feasible in the current headless fixture — see the detailed note above
// render_csf_image / csf_geometry_readback_skip_reason() in Gfx.hpp: the only
// sink here (BackgroundNode) consumes an Image input and reads a texture, so a
// geometry/buffer-only CSF is never dispatched, and there is no readBackBuffer
// sink. Building one is the deferred raw-raster/VSA sink extension.
//
// What we CAN assert here without a GPU readback is the real parse + node-build +
// port-surface path: each geometry/storage CSF must parse, construct a
// score::gfx::ISFNode via the compute constructor, and expose the expected
// Geometry / Buffer output port(s). Then the RENDER readback is SKIPped with the
// precise reason, so the coverage intent is recorded and turns green the day the
// buffer-readback sink lands. (make_csf_node parses with no GPU, so these run
// even headless.)
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/Uniforms.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

int count_outputs(const score::gfx::Node& n, score::gfx::Types t)
{
  int k = 0;
  for(auto* p : n.output)
    if(p->type == t)
      ++k;
  return k;
}

// Build the CSF node (parse + construct, no GPU) inside a booted app and capture
// its output-port surface, so the test can assert on it after the lambda.
struct PortSurface
{
  bool built = false;
  std::string error;
  int geometry_outputs = 0;
  int buffer_outputs = 0;
  int image_outputs = 0;
};

PortSurface build_and_inspect(const QString& csfPath)
{
  PortSurface s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto built = score::test::gfx::make_csf_node(csfPath);
    if(!built.node)
    {
      s.error = built.error;
      return;
    }
    s.built = true;
    s.geometry_outputs = count_outputs(*built.node, score::gfx::Types::Geometry);
    s.buffer_outputs = count_outputs(*built.node, score::gfx::Types::Buffer);
    s.image_outputs = count_outputs(*built.node, score::gfx::Types::Image);
  });
  return s;
}
}

// A non-skipped anchor so Catch2 reports a run rather than exit-4 "no tests ran"
// when every case below ends in a documented SKIP.
TEST_CASE("CSF geometry-readback coverage manifest", "[gfx][l3][csf][geometry][skip]")
{
  CHECK(true);
}

TEST_CASE("csf-vertex-count-expr builds a geometry-output node", "[gfx][l3][csf][geometry]")
{
  const PortSurface s = build_and_inspect(corpus("csf-vertex-count-expr.cs"));

  INFO("error=" << s.error);
  REQUIRE(s.built);
  // A write_only geometry RESOURCE -> exactly one Types::Geometry output port.
  CHECK(s.geometry_outputs == 1);

  SKIP(std::string("geometry readback: ")
       + score::test::gfx::csf_geometry_readback_skip_reason());
}

TEST_CASE("csf-storage-rw builds geometry + storage-buffer outputs", "[gfx][l3][csf][geometry]")
{
  const PortSurface s = build_and_inspect(corpus("csf-storage-rw.cs"));

  INFO("error=" << s.error << " geo=" << s.geometry_outputs
                << " buf=" << s.buffer_outputs);
  REQUIRE(s.built);
  // read_write storage buffer -> a Buffer output; write_only geometry -> a
  // Geometry output.
  CHECK(s.geometry_outputs == 1);
  CHECK(s.buffer_outputs >= 1);

  SKIP(std::string("storage-buffer + geometry readback: ")
       + score::test::gfx::csf_geometry_readback_skip_reason());
}
