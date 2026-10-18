// An INDIRECT dispatch reads a {x, y, z} u32 triplet at the pass's OFFSET in its
// TARGET buffer. Vulkan requires the offset to be a multiple of 4 and the 12
// bytes to lie inside the buffer (VUID-vkCmdDispatchIndirect-offset-02710 /
// -00407); the other APIs have the same rule. The offset was only clamped to
// >= 0 and the size never checked: an out-of-bounds read of the arguments is a
// device fault (shader board C32). Such a pass must run with its WORKGROUPS
// ceiling instead, which the shader is required to bound itself under.
#include <score_test/Gfx.hpp>

#include "GfxLogCapture.hpp"

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
    "an INDIRECT dispatch whose arguments are out of bounds or unaligned runs "
    "its ceiling",
    "[gfx][csf][indirect][C32]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  const auto shader = GENERATE(
      std::string{"csf-indirect-dispatch-short.cs"},
      std::string{"csf-indirect-dispatch-unaligned.cs"});
  CAPTURE(shader);
  IsfResult r;
  std::vector<LogCapture::Message> messages;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    r = render_csf_image(be, corpus(shader.c_str()), {}, {8, 8});
    messages = log.messages();
  });
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs.back();
  REQUIRE(img.valid());

  int red = 0;
  for(int y = 0; y < img.height; y++)
    for(int x = 0; x < img.width; x++)
      red += near(img.at(x, y), {255, 0, 0, 255}, 2);
  CHECK(red == img.width * img.height);

  int validation = 0, warned = 0;
  for(const auto& m : messages)
  {
    if(m.text.contains(QStringLiteral("vkCmdDispatchIndirect")))
      ++validation;
    if(m.text.contains(QStringLiteral("INDIRECT dispatch arguments")))
      ++warned;
  }
  CHECK(validation == 0);
  CHECK(warned >= 1);
}
