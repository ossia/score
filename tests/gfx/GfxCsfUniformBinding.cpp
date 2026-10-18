// A top-level CSF uniform block (RESOURCES TYPE "uniform") takes a binding in
// the shader at its position. buildComputeSrbBindings had no branch for it:
// the block was never bound and every later sampled texture / buffer was one
// binding off (shader board C06). csf-uniform-binding.cs copies a texture
// declared after the block, fed by isf-solid-green.fs, plus the block's tint,
// which reads zeros with nothing cabled.
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + file;
}
}

TEST_CASE(
    "a CSF uniform block is bound and leaves later bindings in place",
    "[gfx][csf][uniform][C06]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_csf_image(
        be, corpus("csf-uniform-binding.cs"), {corpus("isf-solid-green.fs")});
  });
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs.back();
  REQUIRE(img.valid());
  const auto c = img.at(img.width / 2, img.height / 2);
  INFO("center=(" << int(c[0]) << "," << int(c[1]) << "," << int(c[2]) << ","
                  << int(c[3]) << ")");
  CHECK(near(c, {0, 255, 0, 255}, 2));
}
