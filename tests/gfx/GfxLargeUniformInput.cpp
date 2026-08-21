// A disconnected `uniform_input` must get a placeholder sized from the shader's
// own LAYOUT, not from a fixed guess.
//
// An ISF `uniform_input` that no cable feeds still needs a valid descriptor, so
// the node allocates one itself (IsfBindingsBuilder::ensureStorageResources).
// That allocation was a hardcoded 256 bytes -- "covers the camera UBO (240 B)
// and most other small UBOs" -- so any block bigger than that got a buffer that
// stops short of the members the shader reads. Sizing it from `e.size` /
// `aux.size` does not help: those are only ever assigned where a buffer already
// exists, so they are 0 on this path. The size has to come from the LAYOUT,
// laid out by the std140 rules (score::gfx::calculateUniformBlockSize).
//
// isf-large-uniform-input.fs declares 4 mat4 + 1 vec4 = 272 bytes std140 and
// reads the trailing vec4, which lives at offset 256 -- entirely past the end
// of the old placeholder. The placeholder is zero-filled, so a correctly sized
// one makes `big.tail.x + big.tail.w` a deterministic 0 and the shader emits
// (0, 255, 64, 255).
//
// Measured with the fix reverted (placeholder forced back to 256 B):
//   OpenGL: centre pixel comes back (255, 0, 64, 255) -- the trailing vec4 is
//           not the zeros the placeholder promises, so the whole colour flips.
//   Vulkan: unaffected; the out-of-range read yields zeros there.
// So the OpenGL leg is what carries this guard.

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}
}

TEST_CASE(
    "a disconnected uniform_input larger than 256 bytes is fully bound",
    "[gfx][l3][binding][regression]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  bool built = false;
  bool readback_ok = false;
  std::array<uint8_t, 4> center{};
  std::string err;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int node = p.addIsf(corpus("isf-large-uniform-input.fs"));
    if(node < 0)
    {
      err = "node build failed: " + p.error();
      return;
    }

    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(node, 0), p.sinkInput(sink));

    if(!p.create(be))
    {
      if(p.skipped())
        return;
      err = p.error();
      return;
    }
    built = true;
    p.render(3);

    const auto img = p.readback(sink);
    readback_ok = img.valid();
    if(readback_ok)
      center = img.center();
  });

  if(!built && err.empty())
    SKIP("backend unavailable");

  INFO("backend=" << backend_name(be) << " error=" << err);
  REQUIRE(err.empty());
  REQUIRE(built);
  REQUIRE(readback_ok);

  INFO(
      "center=" << int(center[0]) << "," << int(center[1]) << "," << int(center[2])
                << "," << int(center[3]));
  CHECK(near(center, {0, 255, 64, 255}, 2));
}
