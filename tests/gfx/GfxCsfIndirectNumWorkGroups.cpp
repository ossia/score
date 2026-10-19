// gl_NumWorkGroups in a CSF reads a uniform the engine fills before each
// dispatch, because SPIRV-Cross cannot emit the built-in for HLSL. On an
// INDIRECT pass the CPU does not know the count the GPU dispatches, and the
// uniform held the WORKGROUPS ceiling instead (shader board C33). Where the
// backend has the built-in (Vulkan, OpenGL, Metal) the shader must read it.
#include <score_test/Gfx.hpp>

#include "GfxLogCapture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

TEST_CASE(
    "gl_NumWorkGroups in an INDIRECT pass is the dispatched count",
    "[gfx][csf][indirect][C33]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_csf_image(
        be, QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-indirect-numworkgroups.cs"), {},
        {8, 8});
  });
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs.back();
  REQUIRE(img.valid());

  // Only the dispatched texels are written (a fresh storage image is not
  // cleared): count the 3x2 block, and texels holding the ceiling.
  int right = 0, ceiling = 0;
  for(int y = 0; y < 2; y++)
    for(int x = 0; x < 3; x++)
    {
      const auto c = img.at(x, y);
      CAPTURE(x, y, int(c[0]), int(c[1]), int(c[2]));
      right += near(c, {3, 2, 1, 255}, 0);
    }
  for(int y = 0; y < img.height; y++)
    for(int x = 0; x < img.width; x++)
      ceiling += near(img.at(x, y), {8, 8, 1, 255}, 0);
  CHECK(right == 6);
  CHECK(ceiling == 0);
}

// D3D has no built-in: the uniform holds the ceiling, and the user is told.
// SCORE_GFX_NO_NATIVE_NUM_WORKGROUPS takes that path here. Only the warning is
// checked: ShaderCache keeps the bake of the previous case for this source.
TEST_CASE(
    "gl_NumWorkGroups through the uniform in an INDIRECT pass warns",
    "[gfx][csf][indirect][C33]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  qputenv("SCORE_GFX_NO_NATIVE_NUM_WORKGROUPS", "1");
  IsfResult r;
  std::vector<LogCapture::Message> messages;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    r = render_csf_image(
        be, QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-indirect-numworkgroups.cs"), {},
        {8, 8});
    messages = log.messages();
  });
  qunsetenv("SCORE_GFX_NO_NATIVE_NUM_WORKGROUPS");
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  int warned = 0;
  for(const auto& m : messages)
    warned += m.text.contains(QStringLiteral("not the dispatched count"));
  CHECK(warned >= 1);
}
