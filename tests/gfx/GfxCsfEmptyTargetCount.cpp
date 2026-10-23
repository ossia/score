// A 1D_BUFFER pass with no TARGET dispatches one invocation per element of the
// storage output it feeds, the trailing array's element count, as
// `"TARGET": "items"` would. It used the buffer's byte size: 16 times too many
// threads for a vec4 buffer (shader board C26).
//
// csf-empty-target-count.cs: `items` is a 16-byte header then vec4[24], 400
// bytes. Each invocation adds 1 to items.invocations; LOCAL_SIZE 1, so after
// one frame it holds the dispatched thread count: 24, where the byte count
// gave 400.
#include <score_test/Gfx.hpp>
#include <score_test/GfxBufferSink.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

TEST_CASE(
    "an empty 1D_BUFFER TARGET dispatches one invocation per element",
    "[gfx][csf][dispatch][C26]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CsfBufferResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_csf_buffer_readback(
        be, {QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-empty-target-count.cs")},
        {64, 64}, 1);
  });
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.storage.size() == 1);
  const auto words = as_ints(r.storage[0].bytes);
  REQUIRE(words.size() == 100);
  CHECK(words[0] == 24);
  // The elements themselves are all written.
  const auto f = as_floats(r.storage[0].bytes);
  for(int i = 0; i < 24; i++)
  {
    CAPTURE(i);
    CHECK(f[4 + 4 * i] == float(i + 1));
  }
}
