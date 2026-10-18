// Built-in process uniforms every node type must fill:
//   * DATE in a single-pass ISF: SimpleRenderedISFNode never copied the render
//     list's date, so DATE stayed (0, 0, 0, 0) (shader board U22);
//   * SAMPLERATE: ProcessUBO::sampleRate was never written (U21). The executor
//     passes the engine's rate with the tick's timings.
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/ISFNode.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

TEST_CASE("a single-pass ISF reads the date", "[gfx][isf][uniforms][U22]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(be, {QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-date-year.fs")});
  });
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs.back();
  REQUIRE(img.valid());
  CHECK(near(img.at(img.width / 2, img.height / 2), {255, 0, 0, 255}, 2));
}

TEST_CASE("a process node takes the sample rate from its timings", "[gfx][uniforms][U21]")
{
  isf::parser p{
      "", "/*{ \"ISFVSN\": \"2.0\", \"INPUTS\": [] }*/\nvoid main() { gl_FragColor = vec4(SAMPLERATE); }\n",
      450, isf::parser::ShaderType::ISF};
  score::gfx::ISFNode node{
      p.data(), QString::fromStdString(p.vertex()), QString::fromStdString(p.fragment())};
  node.process(score::gfx::Timings{
      .date = ossia::time_value{0}, .parent_duration = {}, .sample_rate = 48000.});
  CHECK(node.standardUBO.sampleRate == 48000.f);
  // A message without a rate (a preview, a step) keeps the last known one.
  node.process(score::gfx::Timings{.date = ossia::time_value{1}});
  CHECK(node.standardUBO.sampleRate == 48000.f);
}
