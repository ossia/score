// A geometry AUXILIARY on a feedback receiver is one buffer that keeps its
// contents from frame to frame (shader board C45).
//
// A CSF whose geometry output is wired back onto its own input owns its
// attributes and ping-pongs them. Its read_write auxiliaries also got a hidden
// twin, swapped every frame, but only one instance is ever bound and the
// shader's `_in`, `_out` and bare names all alias it: the shader saw its real
// buffer one frame and a zeroed copy the next. A counter incremented once per
// frame read 1, 1, 2, 2, 3, 3; looped particle emitters re-initialised their
// pool every second frame.
//
// csf-aux-feedback-counter.cs adds 1 to geo_state.frame per frame; the sink
// reads the auxiliary riding the geometry after each frame.
#include <score_test/Gfx.hpp>
#include <score_test/GfxBufferSink.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstring>

using namespace score::test::gfx;

TEST_CASE(
    "a feedback receiver's auxiliary keeps its contents every frame",
    "[gfx][csf][auxiliary][feedback][C45]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  std::string skip;
  std::vector<uint32_t> frames;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(const char* why = compute_shader_skip_reason(api))
    {
      skip = std::string{backend_name(api)} + ": " + why;
      return;
    }
    GfxPipeline p;
    const int gen = p.addCsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-aux-feedback-counter.cs"));
    REQUIRE(gen >= 0);
    auto sink = attach_buffer_sink(p);
    REQUIRE(sink.node);
    p.wire(p.geometryOut(gen, 0), sink.node->sinkInput());
    p.wireFeedback(p.geometryOut(gen, 0), p.geometryIn(gen, 0));
    sink.node->setSize({64, 64});
    REQUIRE(p.create(api));
    auto state = sink.node->renderState();
    REQUIRE(state);
    REQUIRE(state->rhi);
    if(!state->rhi->isFeatureSupported(QRhi::ReadBackNonUniformBuffer))
    {
      skip = std::string{backend_name(api)} + ": no non-uniform buffer readback";
      return;
    }

    for(int f = 0; f < 8; f++)
    {
      render_with_buffer_sink(p, *sink.node, 1);
      REQUIRE(p.error().empty());
      uint32_t v = 0xffffffffu;
      for(const auto& aux : sink.node->harvest->auxiliaries)
        if(aux.name == "state" && aux.rb.data.size() >= 4)
          std::memcpy(&v, aux.rb.data.constData(), 4);
      frames.push_back(v);
    }
  });
  if(!skip.empty())
    SKIP(skip);

  REQUIRE(frames.size() == 8);
  CAPTURE(frames);
  // Each frame adds exactly one to what the previous frame left.
  for(std::size_t i = 1; i < frames.size(); i++)
  {
    CAPTURE(i);
    CHECK(frames[i] == frames[i - 1] + 1);
  }
  CHECK(frames.front() >= 1);
}
