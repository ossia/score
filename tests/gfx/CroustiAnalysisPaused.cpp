// An analysis node runs while the transport is paused when something reached
// it: a control, a result of its worker.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <QCoreApplication>
#include <QThread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <atomic>
#include <functional>
#include <optional>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
struct PausedProbe
{
  halp_meta(name, "Paused analysis probe")
  halp_meta(c_name, "test_paused_analysis_probe")
  halp_meta(category, "Test")
  halp_meta(uuid, "93d0b6e1-2f7a-4c58-8e04-b1a7c6d95f32")
  struct
  {
    halp::impulse_button<"Go"> go;
    halp::spinbox_i32<"Value", halp::irange{0, 100, 0}> value;
  } inputs;
  struct
  {
    halp::val_port<"Out", int> out;
  } outputs;
  struct worker
  {
    std::function<void(int, std::shared_ptr<std::atomic_int>)> request;
    static std::function<void(PausedProbe&)>
    work(int v, std::shared_ptr<std::atomic_int> side)
    {
      if(side)
      {
        side->store(v);
        return {};
      }
      return [v](PausedProbe& p) { p.result = v; };
    }
  } worker;

  bool sideChannel{};
  std::shared_ptr<std::atomic_int> side = std::make_shared<std::atomic_int>(0);
  int impulses{}, runs{}, value{}, result{}, seenResult{};
  void operator()()
  {
    runs++;
    value = inputs.value.value;
    seenResult = sideChannel ? side->load() : result;
    if(inputs.go)
    {
      impulses++;
      worker.request(42, sideChannel ? side : nullptr);
    }
  }
};

struct Processes
{
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  template <typename T>
  std::unique_ptr<score::gfx::Node> make(const score::DocumentContext& ctx)
  {
    auto model = std::make_unique<oscr::ProcessModel<T>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));
    return std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<T>{*raw, {}, Gfx::exec_controls{}, 1, ctx}};
  }
};

void send(
    score::gfx::Node& n, int64_t date, std::vector<std::optional<ossia::value>> vals = {})
{
  score::gfx::Message m;
  m.node_id = n.nodeId;
  m.token.date = ossia::time_value{date};
  m.token.parent_duration = ossia::time_value{1000};
  for(auto& v : vals)
  {
    m.input.emplace_back();
    if(v)
      m.input.back() = std::move(*v);
  }
  n.process(std::move(m));
}
}

TEST_CASE("an analysis node runs while paused for what reached it", "[gfx][avnd][paused]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool sideChannel = GENERATE(false, true);
  CAPTURE(backend_name(api), sideChannel);

  bool skipped = false;
  std::string error;
  Processes procs;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      error = "no document";
      return;
    }
    GfxPipeline p;
    const int an = p.addNode(procs.make<PausedProbe>(doc->context()));
    p.addSink({8, 8});
    if(!p.create(api))
    {
      skipped = p.skipped();
      error = skipped ? std::string{} : p.error();
      return;
    }
    auto& node = *p.node(an);
    auto* out = dynamic_cast<score::gfx::OutputNode*>(&node);
    oscr::GfxRenderer<PausedProbe>* r{};
    for(auto& [rl, nr] : node.renderedNodes)
      r = static_cast<oscr::GfxRenderer<PausedProbe>*>(nr);
    if(!out || !r)
    {
      error = "no analysis renderer";
      return;
    }
    auto& s = *r->state;
    s.sideChannel = sideChannel;

    send(node, 5);
    out->render();
    const int runsAtStart = s.runs;
    CHECK(runsAtStart == 1);

    out->render();
    out->render();
    CHECK(s.runs == runsAtStart);

    send(node, 5, {std::nullopt, ossia::value{7}});
    out->render();
    CHECK(s.value == 7);

    send(node, 5, {ossia::value{ossia::impulse{}}});
    out->render();
    CHECK(s.impulses == 1);

    for(int i = 0; i < 200 && s.seenResult != 42; i++)
    {
      QCoreApplication::processEvents();
      QThread::msleep(5);
      out->render();
    }
    CHECK(s.seenResult == 42);

    out->render();
    const int settled = s.runs;
    out->render();
    out->render();
    CHECK(s.runs == settled);

    send(node, 6);
    out->render();
    CHECK(s.runs == settled + 1);
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << error);
  REQUIRE(error.empty());
}
