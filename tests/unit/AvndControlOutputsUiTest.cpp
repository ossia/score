// The control values of an avnd process reach its model ports through a
// triple buffer: the interface gets the latest one, the audio thread does not
// allocate for it once warm, and nothing is copied when the interface does
// not listen.

#include <Process/Dataflow/Port.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>

#include <Crousti/Executor.hpp>
#include <Crousti/ExecutorUpdateControlValueInUi.hpp>
#include <Crousti/ProcessModel.hpp>
#include <Ui/VUMeter.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <algorithm>

using Catch::Approx;

namespace
{
using VU = Ui::VUMeter::Node;
const QString vu_uuid = QStringLiteral("0d0a3152-8ee9-4472-8a97-457b8bd6e56a");

void run_exec(Execution::DocumentPlugin& plug)
{
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  plug.runAllCommands();
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
}

struct vu_exec
{
  Process::ProcessModel& proc;
  Execution::DocumentPlugin& plug;
  std::shared_ptr<Execution::ProcessComponent> comp;
  std::shared_ptr<oscr::safe_node<VU>> node;

  //! One buffer of a constant `level` on two channels.
  void tick(double level, int frames = 64)
  {
    auto* in = static_cast<ossia::audio_inlet*>(node->root_inputs()[0]);
    (*in)->set_channels(2);
    for(auto& c : (*in)->get())
      c.assign(frames, level);

    ossia::token_request tk;
    tk.prev_date = ossia::time_value{0};
    tk.date = ossia::time_value{frames};
    tk.start_sample = 0;
    tk.length_sample = frames;

    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    node->run(tk, ossia::exec_state_facade{plug.context().execState.get()});
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    static_cast<ossia::value_outlet*>(node->root_outputs()[0])->data.clear();
  }

  //! What the coarse update timer does.
  void update_ui()
  {
    oscr::update_control_value_in_ui<VU>{
        node, static_cast<oscr::ProcessModel<VU>*>(&proc)}();
  }

  Process::ControlOutlet& levels()
  {
    auto* o = qobject_cast<Process::ControlOutlet*>(proc.outlets()[0]);
    REQUIRE(o);
    return *o;
  }
};

template <typename F>
void with_vu(F&& f)
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, vu_uuid, {});
    REQUIRE(proc);

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    run_exec(plug);

    REQUIRE(plug.baseScenario());
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto it = procs.find(proc->id());
    REQUIRE(it != procs.end());
    auto node = std::dynamic_pointer_cast<oscr::safe_node<VU>>(it->second->node);
    REQUIRE(node);

    vu_exec e{*proc, plug, it->second, node};
    f(e);
  });
}
}

TEST_CASE("The interface gets the latest control output", "[avnd][telemetry]")
{
  with_vu([](vu_exec& e) {
    e.tick(0.25);
    e.tick(0.5);
    e.update_ui();

    auto v = e.levels().value().target<std::vector<ossia::value>>();
    REQUIRE(v);
    // Two channels of peak, rms and held peak.
    REQUIRE(v->size() == 6);
    CHECK(ossia::convert<float>((*v)[0]) == Approx(0.5));
    CHECK(ossia::convert<float>((*v)[3]) == Approx(0.5));
  });
}

TEST_CASE("Control outputs stop allocating once every slot is warm", "[avnd][telemetry]")
{
  with_vu([](vu_exec& e) {
    for(int i = 0; i < 4; i++)
    {
      e.tick(0.1);
      e.update_ui();
    }

    std::vector<const float*> seen;
    for(int i = 0; i < 12; i++)
    {
      seen.push_back(std::get<0>(e.node->control.outs_buffer.write_buffer()).data());
      e.tick(0.1 * i);
      if(i % 3 == 0)
        e.update_ui();
    }
    std::sort(seen.begin(), seen.end());
    seen.erase(std::unique(seen.begin(), seen.end()), seen.end());
    CHECK(seen.size() <= 3);
  });
}

TEST_CASE("Nothing is copied for an interface that does not listen", "[avnd][telemetry]")
{
  with_vu([](vu_exec& e) {
    e.node->control.notify_ui = false;
    e.tick(0.5);
    CHECK(!e.node->control.outs_buffer.has_new_data());
  });
}
