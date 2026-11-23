// A CPU texture input declaring request_readback is only read back, and the
// frame only waits for the GPU, when it asks for it.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Gfx/Graph/TexgenNode.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstring>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
struct GatedInput : halp::texture_input<"Gated">
{
  bool request_readback{};
};

struct GatedFilter
{
  halp_meta(name, "Gated readback filter")
  halp_meta(c_name, "test_gated_readback_filter")
  halp_meta(category, "Test")
  halp_meta(uuid, "0b2f7c61-5a3e-4d18-9c47-e1a6d8f3b250")
  struct
  {
    GatedInput image;
  } inputs;
  struct
  {
    halp::texture_output<"Out"> image;
  } outputs;
  int seen{};
  void operator()()
  {
    seen += inputs.image.texture.bytes != nullptr;
    outputs.image.create(4, 4);
    std::memset(outputs.image.texture.bytes, 255, 4 * 4 * 4);
    outputs.image.upload();
  }
};

struct UngatedFilter
{
  halp_meta(name, "Ungated readback filter")
  halp_meta(c_name, "test_ungated_readback_filter")
  halp_meta(category, "Test")
  halp_meta(uuid, "6d41a8e2-03bf-4c75-8e19-2f7b9c5d0a83")
  struct
  {
    halp::texture_input<"In"> image;
  } inputs;
  struct
  {
    halp::texture_output<"Out"> image;
  } outputs;
  int seen{};
  void operator()()
  {
    seen += inputs.image.texture.bytes != nullptr;
    outputs.image.create(4, 4);
    std::memset(outputs.image.texture.bytes, 255, 4 * 4 * 4);
    outputs.image.upload();
  }
};

struct MixedFilter
{
  halp_meta(name, "Mixed readback filter")
  halp_meta(c_name, "test_mixed_readback_filter")
  halp_meta(category, "Test")
  halp_meta(uuid, "c95e0d37-7b12-46af-a3d8-48e1f6b2c907")
  struct
  {
    GatedInput gated;
    halp::texture_input<"Ungated"> ungated;
  } inputs;
  struct
  {
    halp::texture_output<"Out"> image;
  } outputs;
  int seen{};
  void operator()()
  {
    seen += inputs.gated.texture.bytes != nullptr;
    outputs.image.create(4, 4);
    std::memset(outputs.image.texture.bytes, 255, 4 * 4 * 4);
    outputs.image.upload();
  }
};

struct GatedAnalysis
{
  halp_meta(name, "Gated readback analysis")
  halp_meta(c_name, "test_gated_readback_analysis")
  halp_meta(category, "Test")
  halp_meta(uuid, "4e8a1b73-d26c-4f90-b5e3-91c07a6f2d18")
  struct
  {
    GatedInput image;
  } inputs;
  struct
  {
    halp::val_port<"Seen", int> seen;
  } outputs;
  int seen{};
  void operator()() { seen += inputs.image.texture.bytes != nullptr; }
};

static_assert(oscr::texture_inputs_storage<GatedFilter>::readbackGated());
static_assert(!oscr::texture_inputs_storage<UngatedFilter>::readbackGated());
static_assert(oscr::texture_inputs_storage<MixedFilter>::readbackGated());

struct Grey : score::gfx::TexgenNode
{
  Grey()
  {
    function = +[](unsigned char* rgba, int w, int h, int) {
      std::memset(rgba, 128, std::size_t(w) * h * 4);
    };
  }
};

struct Processes
{
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  int next = 1;

  template <typename T>
  std::unique_ptr<score::gfx::Node> make(const score::DocumentContext& ctx)
  {
    auto model = std::make_unique<oscr::ProcessModel<T>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{next}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));
    return std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<T>{*raw, {}, Gfx::exec_controls{}, next++, ctx}};
  }
};

template <typename T>
oscr::GfxRenderer<T>* rendererOf(score::gfx::Node& n)
{
  for(auto& [rl, r] : n.renderedNodes)
    if(r)
      return static_cast<oscr::GfxRenderer<T>*>(r);
  return nullptr;
}

struct Outcome
{
  bool skipped{};
  std::string error;
  bool waited{};
  int seen{};
};

//! Renders texgen(s) -> T -> sink; `wire[k]` says whether input k is wired,
//! `request` the gated input's request_readback.
template <typename T>
Outcome run(score::gfx::GraphicsApi api, std::vector<bool> wire, bool request)
{
  Outcome o;
  Processes procs;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      o.error = "no document";
      return;
    }
    GfxPipeline p;
    const int flt = p.addNode(procs.make<T>(doc->context()));
    for(std::size_t k = 0; k < wire.size(); k++)
      if(wire[k])
      {
        const int tg = p.addNode(std::make_unique<Grey>());
        p.wire(p.nodeImageOut(tg, 0), p.node(flt)->input[k]);
      }
    const int sink = p.addSink({16, 16});
    p.wire(p.nodeImageOut(flt, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      o.skipped = p.skipped();
      o.error = o.skipped ? std::string{} : p.error();
      return;
    }
    auto* r = rendererOf<T>(*p.node(flt));
    if(!r)
    {
      o.error = "no renderer";
      return;
    }
    if constexpr(requires { r->state->inputs.image.request_readback; })
      r->state->inputs.image.request_readback = request;
    else if constexpr(requires { r->state->inputs.gated.request_readback; })
      r->state->inputs.gated.request_readback = request;
    p.render(4);
    o.seen = r->state->seen;
    o.waited = r->m_waitedForReadback;
  });
  return o;
}

#define GATE_REQUIRE_LIVE(o)     \
  if((o).skipped)                \
    SKIP("backend unavailable"); \
  INFO("error=" << (o).error);   \
  REQUIRE((o).error.empty())
}

TEST_CASE("a gated CPU texture input is read back only on request", "[gfx][avnd][readback]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  SECTION("declined")
  {
    const auto o = run<GatedFilter>(api, {true}, false);
    GATE_REQUIRE_LIVE(o);
    CHECK_FALSE(o.waited);
    CHECK(o.seen == 0);
  }
  SECTION("requested")
  {
    const auto o = run<GatedFilter>(api, {true}, true);
    GATE_REQUIRE_LIVE(o);
    CHECK(o.waited);
    CHECK(o.seen > 0);
  }
  SECTION("requested but not wired")
  {
    const auto o = run<GatedFilter>(api, {false}, true);
    GATE_REQUIRE_LIVE(o);
    CHECK_FALSE(o.waited);
    CHECK(o.seen == 0);
  }
}

TEST_CASE("an ungated CPU texture input always waits for its readback", "[gfx][avnd][readback]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto wired = run<UngatedFilter>(api, {true}, false);
  GATE_REQUIRE_LIVE(wired);
  CHECK(wired.waited);
  CHECK(wired.seen > 0);

  const auto unwired = run<UngatedFilter>(api, {false}, false);
  GATE_REQUIRE_LIVE(unwired);
  CHECK(unwired.waited);
}

TEST_CASE(
    "a gated and an ungated input together wait only for a queued readback",
    "[gfx][avnd][readback]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto ungatedWired = run<MixedFilter>(api, {true, true}, false);
  GATE_REQUIRE_LIVE(ungatedWired);
  CHECK(ungatedWired.waited);
  CHECK(ungatedWired.seen == 0);

  const auto nothingWired = run<MixedFilter>(api, {true, false}, false);
  GATE_REQUIRE_LIVE(nothingWired);
  CHECK_FALSE(nothingWired.waited);

  const auto gatedRequested = run<MixedFilter>(api, {true, false}, true);
  GATE_REQUIRE_LIVE(gatedRequested);
  CHECK(gatedRequested.waited);
  CHECK(gatedRequested.seen > 0);
}

namespace
{
Outcome runAnalysis(score::gfx::GraphicsApi api, bool wire, bool request)
{
  Outcome o;
  Processes procs;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      o.error = "no document";
      return;
    }
    GfxPipeline p;
    const int an = p.addNode(procs.make<GatedAnalysis>(doc->context()));
    if(wire)
    {
      const int tg = p.addNode(std::make_unique<Grey>());
      p.wire(p.nodeImageOut(tg, 0), p.node(an)->input[0]);
    }
    const int sink = p.addSink({16, 16});
    (void)sink;
    if(!p.create(api))
    {
      o.skipped = p.skipped();
      o.error = o.skipped ? std::string{} : p.error();
      return;
    }
    auto* out = dynamic_cast<score::gfx::OutputNode*>(p.node(an));
    auto* r = rendererOf<GatedAnalysis>(*p.node(an));
    if(!out || !r)
    {
      o.error = "no analysis renderer";
      return;
    }
    r->state->inputs.image.request_readback = request;
    for(int f = 0; f < 4; f++)
    {
      p.render(1);
      out->render();
    }
    o.seen = r->state->seen;
    o.waited = r->m_waitedForReadback;
  });
  return o;
}
}

TEST_CASE("an analysis node's gated input is read back only on request", "[gfx][avnd][readback]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto declined = runAnalysis(api, true, false);
  GATE_REQUIRE_LIVE(declined);
  CHECK_FALSE(declined.waited);
  CHECK(declined.seen == 0);

  const auto requested = runAnalysis(api, true, true);
  GATE_REQUIRE_LIVE(requested);
  CHECK(requested.waited);
  CHECK(requested.seen > 0);

  const auto unwired = runAnalysis(api, false, true);
  GATE_REQUIRE_LIVE(unwired);
  CHECK_FALSE(unwired.waited);
}
