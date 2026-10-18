// 1D_BUFFER dispatch sizing: the workgroup count is ceil(items / (workgroup
// size * STRIDE_X)), and the product was an int: LOCAL_SIZE 1024 with STRIDE_X
// 4194304 wrapped to 0 and divided by it (shader board C36).
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

TEST_CASE(
    "a 1D_BUFFER pass whose workgroup size times stride passes int dispatches",
    "[gfx][csf][dispatch][C36]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_csf_image(
        be, QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-dispatch-stride-overflow.cs"), {},
        {8, 8});
  });
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs.back();
  REQUIRE(img.valid());
  CHECK(near(img.at(4, 4), {255, 0, 0, 255}, 2));
}
