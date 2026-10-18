// Shader board F16: a classic ISF draws its fullscreen mesh, so a
// PIPELINE_STATE VERTEX_COUNT / INSTANCE_COUNT was parsed and silently ignored:
// isf-vertex-count.fs asks for 64 points placed on an 8x8 grid by
// gl_VertexIndex and lit 4 cells (GL, Vulkan, 2026-10-18). The owner's
// decision is to refuse those keys in classic ISF: the shader does not load,
// and says why, instead of drawing something else than it asked for.
// tests/unit/IsfHeaderValidationTest.cpp covers the parser side. The shader
// lives in corpus/refused/, which the corpus sweep (ShaderCorpusTargets) does
// not bake.
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

TEST_CASE(
    "a classic ISF with a PIPELINE_STATE VERTEX_COUNT is refused", "[gfx][isf][F16]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(
        be, {QStringLiteral(GFX_TEST_CORPUS_DIR "/refused/isf-vertex-count.fs")}, {64, 64});
  });
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  CHECK(!r.error.empty());
  CHECK(r.error.find("VERTEX_COUNT") != std::string::npos);
}
