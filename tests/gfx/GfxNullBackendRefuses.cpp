// =============================================================================
// P2-15 — the Null backend refuses rather than pretends.
//
// QRhi's Null backend accepts every command and rasterizes nothing. That makes
// it a legitimate target for the half of a case that is a DECISION -- which shim
// was selected, which pass was recorded, which cap was queried -- and an
// illegitimate target for anything that reads a pixel back.
//
// Before this file the fixture did neither. `SCORE_TEST_API=null ctest -R gfx`
// ran every pixel case all the way through create() and render(), and then died
// on
//     "readback of output 0 was empty/short (got 0 bytes for 64x64)"
// which is a RED that names the fixture rather than the code under test: on a
// platform triage run it is indistinguishable from a real regression. The
// symmetric hazard is worse and quieter -- a case whose expected colour is the
// zero pixel would have gone GREEN against a buffer nothing ever drew into.
//
// The contract this file pins, and which score_test/Gfx.hpp now implements:
//
//   1. SCORE_TEST_API=null selects GraphicsApi::Null and nothing else. Pure
//      decision logic: no app, no GPU, no pixels. It runs on every host and on
//      every backend, including under SCORE_TEST_API=null itself.
//   2. Every pixel-producing fixture entry point (render_isf_chain and
//      everything funnelling through GfxPipeline::create) reports
//      skipped=true with a reason naming Null, an EMPTY error and NO outputs.
//      The `if(r.skipped) SKIP(...)` prologue every case already carries turns
//      that into a SKIP verdict rather than a red or a lucky green.
//   3. Opting back in (GfxPipeline::allowNullBackend(), used by
//      GfxCubemapSixFaces' shim-selection half) lets the DECISION assertions
//      run: the graph builds, the sink gets a render state, the node's ports
//      are real.
//   4. A real backend is untouched: it must never report the Null reason.
//
// (3) also measures WHY the refusal has to sit at create() rather than at the
// readback, and the measurement contradicted the draft of this file.
//
// The draft assumed Null would hand back an empty buffer no assertion could be
// fooled by. It does not. QRhi's Null backend services readBackTexture with a
// correctly-sized, correctly-strided RGBA8 image: valid() is TRUE, at() works,
// and every one of the 4096 pixels carries ONE value -- measured on this host,
// Qt 6.12 dev build, that value is opaque YELLOW (255,255,0,255), not even the
// black an unrendered target would suggest. Nothing anywhere in the result
// distinguishes it from a real frame.
//
// So a pixel case on Null does not fail to get an answer, it gets a WRONG one,
// and any case whose expectation happens to be that uniform value -- or which
// only asks "is the frame non-blank?", which this frame passes -- goes green
// against a picture that never existed. That is the "pretends" this case is
// named for. It is asserted here rather than asserted away, and refusing before
// the render is the only place it cannot be reached by accident.
//
// NEGATIVE CONTROL (run, see the ledger): delete the `api == score::gfx::Null`
// refusal from GfxPipeline::create() in tests/fixtures/score_test/Gfx.hpp. Case
// 2's GfxPipeline half then stops skipping and the case goes RED on
// `p.skipped()`, which is exactly the state the whole suite was in before.
//
// Run:
//   DISPLAY=:0 ctest -R gfx_null_backend_refuses
//   DISPLAY=:0 SCORE_TEST_API=null ctest -R gfx_null_backend_refuses
// =============================================================================

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QByteArray>

#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

//! Restore SCORE_TEST_API on scope exit: case 1 rewrites it, and the rest of
//! the suite (and the rest of THIS file) reads it through platform_backends().
struct ScopedTestApi
{
  bool was_set = qEnvironmentVariableIsSet("SCORE_TEST_API");
  QByteArray old = qgetenv("SCORE_TEST_API");
  ~ScopedTestApi()
  {
    if(was_set)
      qputenv("SCORE_TEST_API", old);
    else
      qunsetenv("SCORE_TEST_API");
  }
};

bool mentions_null(const std::string& reason)
{
  return reason.find("Null") != std::string::npos;
}
}

// -----------------------------------------------------------------------------
// 1. Decision logic. No app, no GPU, no pixels — this half is what must keep
//    running when SCORE_TEST_API=null makes everything else skip.
// -----------------------------------------------------------------------------
TEST_CASE(
    "SCORE_TEST_API=null selects the Null backend and only it",
    "[gfx][null][decision]")
{
  ScopedTestApi guard;

  qputenv("SCORE_TEST_API", "null");
  const auto sel = platform_backends();
  REQUIRE(sel.size() == 1);
  CHECK(sel[0] == score::gfx::Null);
  CHECK(std::string(backend_name(sel[0])) == "Null");

  // The neighbouring selectors still mean what they say — a typo'd branch that
  // fell through to Null would be exactly the "silently ran on Null" failure
  // this case exists to prevent.
  qputenv("SCORE_TEST_API", "opengl");
  REQUIRE(platform_backends().size() == 1);
  CHECK(platform_backends()[0] == score::gfx::OpenGL);

  qputenv("SCORE_TEST_API", "vulkan");
  REQUIRE(platform_backends().size() == 1);
  CHECK(platform_backends()[0] == score::gfx::Vulkan);

  // An unset variable must NOT resolve to Null on any platform: the default
  // sweep is the real backends.
  qunsetenv("SCORE_TEST_API");
  const auto dflt = platform_backends();
  REQUIRE(!dflt.empty());
  for(auto api : dflt)
    CHECK(api != score::gfx::Null);
}

// -----------------------------------------------------------------------------
// 2. Every pixel-producing entry point refuses Null with a SKIP verdict.
// -----------------------------------------------------------------------------
TEST_CASE(
    "a pixel case asked for Null reports SKIP, not a pass and not a red",
    "[gfx][null][decision]")
{
  IsfResult chain;
  bool pipeline_created = true;
  bool pipeline_skipped = false;
  std::string pipeline_reason;
  std::string pipeline_error;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    // (a) the linear-chain entry point
    chain = render_isf_chain(score::gfx::Null, {corpus("isf-solid-color.fs")});

    // (b) the general driver every other helper funnels through
    GfxPipeline p;
    const int n = p.addIsf(corpus("isf-solid-color.fs"));
    const int s = p.addSink({64, 64});
    p.wire(p.imageOut(n, 0), p.sinkInput(s));
    pipeline_created = p.create(score::gfx::Null);
    pipeline_skipped = p.skipped();
    pipeline_reason = p.skipReason();
    pipeline_error = p.error();
  });

  INFO("chain reason: " << chain.skip_reason);
  CHECK(chain.skipped);
  CHECK(mentions_null(chain.skip_reason));
  // A skip is not a failure: reporting BOTH would make the verdict ambiguous,
  // and an error string here is what the pre-P2-15 behaviour produced.
  CHECK(chain.error.empty());
  // And there is no image to read: a case that ignored `skipped` still cannot
  // obtain a buffer and mistake it for a frame.
  CHECK(chain.outputs.empty());

  INFO("pipeline reason: " << pipeline_reason);
  CHECK_FALSE(pipeline_created);
  CHECK(pipeline_skipped);
  CHECK(mentions_null(pipeline_reason));
  CHECK(pipeline_error.empty());
}

// -----------------------------------------------------------------------------
// 3. Opting in runs the decisions and still yields no pixels.
// -----------------------------------------------------------------------------
TEST_CASE(
    "opting into Null runs the decision assertions, and its readback is a "
    "plausible frame nothing drew",
    "[gfx][null][decision]")
{
  bool created = false;
  bool skipped = false;
  std::string reason, error, backend;

  // Decisions available without a single pixel.
  bool has_image_output = false;
  bool sink_has_render_state = false;
  ReadbackImage img;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    p.allowNullBackend();

    const int n = p.addIsf(corpus("isf-solid-color.fs"));
    if(n < 0)
    {
      error = p.error();
      return;
    }
    has_image_output = (p.imageOut(n, 0) != nullptr);

    const int s = p.addSink({64, 64});
    p.wire(p.imageOut(n, 0), p.sinkInput(s));

    created = p.create(score::gfx::Null);
    skipped = p.skipped();
    reason = p.skipReason();
    error = p.error();
    backend = p.backend();
    if(!created)
      return;

    sink_has_render_state = (p.sink(s)->renderState() != nullptr);
    p.render(3);
    img = p.readback(s);
  });

  INFO("backend=" << backend << " error=" << error << " reason=" << reason);
  if(!created && skipped)
    SKIP("Null QRhi could not be created here: " << reason);

  REQUIRE(error.empty());
  REQUIRE(created);

  // The decision half ran and passed.
  CHECK(has_image_output);
  CHECK(sink_has_render_state);
  CHECK(backend == "Null");

  // ...and the pixel half is exactly why create() refuses by default.
  //
  // MEASURED, and worse than "empty": the readback is a perfectly well-formed
  // 64x64 RGBA8 frame — valid() TRUE, right stride, at() works — every pixel of
  // which carries one value because nothing was drawn. The uniformity is what
  // is asserted; the particular value is Qt's and is only reported (INFO), so
  // this case does not break when Qt changes it.
  REQUIRE(img.valid());
  REQUIRE(img.width == 64);
  REQUIRE(img.height == 64);

  const auto first = img.at(0, 0);
  int distinct = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
      if(!near(img.at(x, y), first, 0))
        ++distinct;

  INFO("Null readback pixel 0,0 = " << int(first[0]) << "," << int(first[1])
                                    << "," << int(first[2]) << ","
                                    << int(first[3]) << "; pixels differing "
                                    << "from it = " << distinct);
  CHECK(distinct == 0);
}

// -----------------------------------------------------------------------------
// 4. A real backend is untouched by the refusal.
// -----------------------------------------------------------------------------
TEST_CASE(
    "the Null refusal does not leak onto a real backend",
    "[gfx][null][decision]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  if(be == score::gfx::Null)
    SKIP("SCORE_TEST_API=null: there is no real backend to check here");

  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(be, {corpus("isf-solid-color.fs")});
  });

  INFO("backend=" << r.backend << " error=" << r.error
                  << " reason=" << r.skip_reason);
  // It may legitimately skip (no driver on this host) — but never for the Null
  // reason, and it must never come back CLAIMING to be Null.
  CHECK_FALSE(mentions_null(r.skip_reason));
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);

  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  CHECK(r.outputs[0].valid());
  CHECK(r.backend != "Null");
}
