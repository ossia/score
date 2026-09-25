// A gpp shader node downstream of an ISF node with an unsized image input
// (agent L4).
//
// Building the render list sizes the ISF inlet from its downstream sinks and
// asks the gpp renderer for its input render target before that renderer's
// initState has run. The renderer answers with an empty target, so the inlet
// falls back to the render size and the graph renders.
//
// Registration: see the test_gfx_gpp_rt_before_init_l4 target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Crousti/GpuNode.hpp>

#include <Gfx/Graph/RenderList.hpp>

#include <gpp/layout.hpp>
#include <gpp/meta.hpp>
#include <gpp/ports.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>
#include <string>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

struct CenterSample
{
  halp_meta(name, "L4 center sample")
  halp_meta(uuid, "e2a7c4d9-6f13-4b58-9c0e-8d3b1f5a7e26")

  struct layout
  {
    enum
    {
      graphics
    };

    struct fragment_output
    {
      gpp_attribute(0, fragColor, float[4])
      fragColor;
    } fragment_output;

    struct bindings
    {
      gpp::sampler<"tex", 1> tex;
    };
  };

  struct
  {
    gpp::texture_input_port<"In", &layout::bindings::tex> in;
  } inputs;

  struct
  {
    gpp::color_attachment_port<"Out", &layout::fragment_output> out;
  } outputs;

  std::string_view fragment()
  {
    return R"_(
void main() {
  fragColor = texture(tex, vec2(0.5));
}
)_";
  }
};

struct Result
{
  bool skipped{};
  std::string skipReason;
  std::string error;
  QSize inletSize;
  std::array<uint8_t, 4> center{};
};

// solid colour -> passthrough (unsized image inlet) -> gpp node -> sink 16x16
Result run(score::gfx::GraphicsApi api)
{
  Result out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    GfxPipeline p;
    const int solid = p.addIsf(corpus("isf-solid-color.fs"));
    const int pass = p.addIsf(corpus("isf-passthrough-plain.fs"));
    auto owned = std::make_unique<oscr::CustomGpuNode<CenterSample>>(
        std::weak_ptr<Execution::ExecutionCommandQueue>{}, Gfx::exec_controls{}, 1,
        doc->context());
    auto* gpp = owned.get();
    const int idx = p.addNode(std::move(owned));
    if(solid < 0 || pass < 0 || idx < 0)
    {
      out.error = "build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({16, 16});
    p.wire(p.imageOut(solid, 0), p.imageIn(pass, 0));
    p.wire(p.imageOut(pass, 0), gpp->input[0]);
    p.wire(gpp->output[0], p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skipReason = p.skipReason();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    const auto& rls = p.graph().renderLists();
    if(rls.empty() || !rls.front())
    {
      out.error = "no render list";
      return;
    }
    if(auto* tex = rls.front()->renderTargetForInputPort(*p.imageIn(pass, 0)).texture)
      out.inletSize = tex->pixelSize();
    auto img = p.readback(sink);
    if(img.valid())
      out.center = img.center();
  });
  return out;
}
}

TEST_CASE(
    "An unsized ISF image inlet feeding a gpp shader node builds and renders",
    "[gfx][avnd][gpp][texture][l4]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Result r = run(api);
  if(r.skipped)
    SKIP(r.skipReason);
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  CHECK(r.inletSize == QSize(16, 16));
  if(api == score::gfx::Null)
    return;
  INFO(
      "center " << int(r.center[0]) << "," << int(r.center[1]) << ","
                << int(r.center[2]) << "," << int(r.center[3]));
  CHECK(r.center[0] > 200);
  CHECK(r.center[1] < 40);
  CHECK(r.center[2] > 200);
}
