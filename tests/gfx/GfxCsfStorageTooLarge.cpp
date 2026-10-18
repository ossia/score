// A storage buffer whose LAYOUT and size control ask for more bytes than can be
// allocated: 536870911 vec4 (the size control's maximum) is 8 GiB. The buffer
// was silently created at INT_MAX bytes and the shader run against a fraction
// of what it declares; a failed regrow kept the old buffer and recorded the new
// size (shader board C13). The pass must not run on a buffer smaller than its
// declaration, and the user must be told.
//
// csf-storage-too-large.cs only reads the bound length, it never writes the
// buffer, so the old behaviour shows as a red image rather than as
// out-of-bounds GPU writes.
#include <score_test/Gfx.hpp>

#include "GfxLogCapture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

TEST_CASE(
    "a CSF storage buffer too large to allocate stops its passes with a warning",
    "[gfx][csf][storage][C13]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  struct
  {
    bool skipped{};
    std::string error;
    ReadbackImage img;
    std::vector<LogCapture::Message> messages;
  } r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    GfxPipeline p;
    const int csf
        = p.addCsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-storage-too-large.cs"));
    if(csf < 0)
    {
      r.error = p.error();
      return;
    }
    const int sink = p.addSink({8, 8});
    p.wire(p.imageOut(csf, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    setControl(*p.isf(csf), nth_control_input(*p.isf(csf), 0), ossia::value{536870911});
    p.render(3);
    r.img = p.readback(sink);
    r.messages = log.messages();
  });
  if(r.skipped)
    SKIP(backend_name(be) << ": no usable backend");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.img.valid());

  int red = 0;
  for(int y = 0; y < r.img.height; y++)
    for(int x = 0; x < r.img.width; x++)
      red += near(r.img.at(x, y), {255, 0, 0, 255}, 2);
  CHECK(red == 0);

  int warned = 0;
  for(const auto& m : r.messages)
    warned += m.text.contains(QStringLiteral("cannot be allocated"));
  CHECK(warned >= 1);
}
