// An object's node_state is one per node: the objects of every renderer of the
// node share it, and it outlives their rebuilds.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Gfx/Graph/TexgenNode.hpp>

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
#include <memory>
#include <set>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
struct SharedCounter
{
  int runs{};
};

struct NodeStateFilter
{
  halp_meta(name, "Node state filter")
  halp_meta(c_name, "test_node_state_filter")
  halp_meta(category, "Test")
  halp_meta(uuid, "e2c84a1f-6b39-4d07-95fa-3c1d8e7b0a64")
  struct
  {
    halp::texture_input<"In"> image;
  } inputs;
  struct
  {
    halp::texture_output<"Out"> image;
  } outputs;
  std::shared_ptr<SharedCounter> node_state;
  void operator()()
  {
    if(node_state)
      node_state->runs++;
    outputs.image.create(4, 4);
    std::memset(outputs.image.texture.bytes, 255, 4 * 4 * 4);
    outputs.image.upload();
  }
};

struct ImpulseFilter
{
  halp_meta(name, "Impulse filter")
  halp_meta(c_name, "test_node_state_impulse_filter")
  halp_meta(category, "Test")
  halp_meta(uuid, "7f1d3e95-a24c-4b60-8d17-c5e9b0a3f241")
  struct
  {
    halp::impulse_button<"Go"> go;
  } inputs;
  struct
  {
    halp::texture_output<"Out"> image;
  } outputs;
  int impulses{};
  void operator()()
  {
    if(inputs.go)
      impulses++;
    outputs.image.create(4, 4);
    std::memset(outputs.image.texture.bytes, 255, 4 * 4 * 4);
    outputs.image.upload();
  }
};

struct Grey : score::gfx::TexgenNode
{
  Grey()
  {
    function = +[](unsigned char* rgba, int w, int h, int) {
      std::memset(rgba, 128, std::size_t(w) * h * 4);
    };
  }
};

std::set<SharedCounter*> statesOf(score::gfx::Node& n, int& renderers)
{
  std::set<SharedCounter*> out;
  renderers = 0;
  for(auto& [rl, r] : n.renderedNodes)
    if(r)
    {
      renderers++;
      out.insert(static_cast<oscr::GfxRenderer<NodeStateFilter>*>(r)->state->node_state.get());
    }
  return out;
}
}

TEST_CASE("the renderers of a node share its node_state", "[gfx][avnd][node_state]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string error;
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      error = "no document";
      return;
    }
    auto model = std::make_unique<oscr::ProcessModel<NodeStateFilter>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, doc->context(), nullptr);
    auto& m = *model;
    models.push_back(std::move(model));

    GfxPipeline p;
    const int tg = p.addNode(std::make_unique<Grey>());
    const int flt = p.addNode(std::unique_ptr<score::gfx::Node>{new oscr::GfxNode<NodeStateFilter>{
        m, {}, Gfx::exec_controls{}, 1, doc->context()}});
    p.wire(p.nodeImageOut(tg, 0), p.node(flt)->input[0]);
    const int a = p.addSink({8, 8});
    const int b = p.addSink({8, 8});
    p.wire(p.nodeImageOut(flt, 0), p.sinkInput(a));
    p.wire(p.nodeImageOut(flt, 0), p.sinkInput(b));
    if(!p.create(api))
    {
      skipped = p.skipped();
      error = skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);

    int renderers{};
    const auto first = statesOf(*p.node(flt), renderers);
    CHECK(renderers == 2);
    REQUIRE(first.size() == 1);
    REQUIRE(*first.begin() != nullptr);
    CHECK((*first.begin())->runs >= 6);

    p.graph().createAllRenderLists(api);
    p.render(1);
    const auto rebuilt = statesOf(*p.node(flt), renderers);
    CHECK(renderers == 2);
    CHECK(rebuilt == first);
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << error);
  REQUIRE(error.empty());
}

TEST_CASE(
    "a renderer created later does not take the impulses its node already took",
    "[gfx][avnd][node_state]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string error;
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      error = "no document";
      return;
    }
    auto model = std::make_unique<oscr::ProcessModel<ImpulseFilter>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, doc->context(), nullptr);
    auto& m = *model;
    models.push_back(std::move(model));

    GfxPipeline p;
    const int flt = p.addNode(std::unique_ptr<score::gfx::Node>{new oscr::GfxNode<ImpulseFilter>{
        m, {}, Gfx::exec_controls{}, 1, doc->context()}});
    const int a = p.addSink({8, 8});
    p.wire(p.nodeImageOut(flt, 0), p.sinkInput(a));
    if(!p.create(api))
    {
      skipped = p.skipped();
      error = skipped ? std::string{} : p.error();
      return;
    }
    auto impulses = [&] {
      int n = -1;
      for(auto& [rl, r] : p.node(flt)->renderedNodes)
        if(r)
          n = static_cast<oscr::GfxRenderer<ImpulseFilter>*>(r)->state->impulses;
      return n;
    };
    p.render(1);
    score::gfx::Message msg;
    msg.node_id = p.node(flt)->nodeId;
    msg.input.emplace_back(ossia::value{ossia::impulse{}});
    p.node(flt)->process(std::move(msg));
    p.render(3);
    CHECK(impulses() == 1);

    p.graph().createAllRenderLists(api);
    p.render(3);
    CHECK(impulses() == 0);
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << error);
  REQUIRE(error.empty());
}
