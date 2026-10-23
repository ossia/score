// The sampler a shader's JSON declares for an image input is the one it gets
// (shader board U34). Ordinary (render-target) inputs used the inlet's
// sampler -- Linear, Repeat by default -- whatever the JSON said; only inputs
// grabbed from their source honoured it.
//
// isf-left-red-right-green.fs -> isf-wrap-clamp-consumer.fs, which samples at
// u = 1.25 with "WRAP": "clamp_to_edge": green (the right edge), where the
// inlet's repeat reads red.
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

TEST_CASE("an image input samples with the WRAP its JSON declares", "[gfx][isf][sampler][U34]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(
        be,
        {QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-left-red-right-green.fs"),
         QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-wrap-clamp-consumer.fs")},
        {32, 32});
  });
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  CHECK(near(r.outputs.back().at(16, 16), {0, 255, 0, 255}, 2));
}
