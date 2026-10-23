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

// A JSON MIPMAP_MODE gives the input a mip chain (shader board U37): the
// render target the upstream draws into is allocated mipmapped and its mips
// generated. It got neither, so sampling a lower level read level 0.
TEST_CASE("an image input with a JSON MIPMAP_MODE has a mip chain", "[gfx][isf][sampler][U37]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(
        be,
        {QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-left-red-right-green.fs"),
         QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-mipmap-lod-consumer.fs")},
        {32, 32});
  });
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto c = r.outputs.back().at(16, 16);
  INFO("rgb=" << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  CHECK(near(c, {128, 128, 0, 255}, 12));
}

// The same precedence for a compute shader's sampled texture (shader board
// C22): its JSON sampler fields were ignored.
TEST_CASE("a CSF texture input samples with the WRAP its JSON declares", "[gfx][csf][sampler][C22]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));
  std::string skip, err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(const char* why = compute_shader_skip_reason(be))
    {
      skip = why;
      return;
    }
    GfxPipeline p;
    const int src = p.addIsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-left-red-right-green.fs"));
    const int csf = p.addCsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-wrap-clamp-consumer.cs"));
    if(src < 0 || csf < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(src, 0), p.imageIn(csf, 0));
    p.wire(p.imageOut(csf, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      skip = p.skipped() ? p.skipReason() : std::string{};
      err = p.skipped() ? std::string{} : p.error();
      return;
    }
    p.render(3);
    img = p.readback(sink);
    if(!img.valid())
      err = "empty readback";
  });
  if(!skip.empty())
    SKIP(skip);
  INFO(err);
  REQUIRE(err.empty());
  CHECK(near(img.at(16, 16), {0, 255, 0, 255}, 2));
}
