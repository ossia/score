// ao::ADSR driven from the UI, on the execution side.
//
// The inspector widgets of Process::ControlWidgets::ImpulseButton and
// ::Button do not write into the execution port: they act on the model inlet,
// whose valueChanged is picked up by oscr::con_unvalidated and applied to the
// object's field in place, from the execution queue. So the object never sees
// a value_port entry and never sees a timestamp -- any edge detection in the
// object has to live in the object's own state.

#include <Process/ExecutionSetup.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>

#include <QApplication>

#include <Avnd/Factories.hpp>
#include <Crousti/Executor.hpp>
#include <Crousti/ProcessModel.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <examples/Advanced/Utilities/ADSR.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

using Catch::Approx;

namespace
{
const QString adsr_uuid = QStringLiteral("2d603ea6-be84-4f8c-8269-643e989f5997");

// ao::ADSR inlets: Hold, Trigger, Attack, Decay, Sustain, Release
constexpr int hold_inlet = 0;
constexpr int trig_inlet = 1;
constexpr int attack_inlet = 2;
constexpr int decay_inlet = 3;
constexpr int release_inlet = 5;

Process::ControlInlet* control(Process::ProcessModel& p, int i)
{
  auto* c = qobject_cast<Process::ControlInlet*>(p.inlets()[i]);
  REQUIRE(c);
  return c;
}

void run_exec(Execution::DocumentPlugin& plug)
{
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  plug.runAllCommands();
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
}

void spin(int ms = 0)
{
  QApplication::processEvents();
  if(ms > 0)
  {
    QElapsedTimer t;
    t.start();
    while(t.elapsed() < ms)
      QApplication::processEvents(QEventLoop::AllEvents, 5);
  }
  QApplication::processEvents();
}

Execution::DocumentPlugin& load_execution(score::Document& doc)
{
  auto& plug = doc.context().plugin<Execution::DocumentPlugin>();
  plug.reload(true, score::test::base_interval(doc));
  run_exec(plug);
  return plug;
}

std::shared_ptr<Execution::ProcessComponent>
component(Execution::DocumentPlugin& plug, const Process::ProcessModel& proc)
{
  REQUIRE(plug.baseScenario());
  auto& procs = plug.baseScenario()->baseInterval().processes();
  auto it = procs.find(proc.id());
  REQUIRE(it != procs.end());
  REQUIRE(it->second);
  REQUIRE(it->second->node);
  return it->second;
}

//! Everything the tests need around one ADSR that is set up for execution.
struct adsr_exec
{
  score::Document& doc;
  Process::ProcessModel& proc;
  Execution::DocumentPlugin& plug;
  std::shared_ptr<Execution::ProcessComponent> comp;
  oscr::safe_node<ao::ADSR>& node;

  ao::ADSR& object() { return node.impl.effect; }

  //! Short envelopes so that the tests stay a handful of buffers long.
  void fast()
  {
    control(proc, attack_inlet)->setValue(0.01f);
    control(proc, decay_inlet)->setValue(0.01f);
    control(proc, release_inlet)->setValue(0.01f);
    run_exec(plug);
  }

  float tick(int frames = 64)
  {
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{0};
    tk.date = ossia::time_value{frames};
    tk.start_sample = 0;
    tk.length_sample = frames;

    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    node.run(tk, ossia::exec_state_facade{plug.context().execState.get()});
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

    auto* out = static_cast<ossia::value_outlet*>(node.root_outputs()[0]);
    float v = 0.f;
    auto& data = (*out)->get_data();
    if(!data.empty())
      v = ossia::convert<float>(data.back().value);
    (*out)->clear();
    return v;
  }

  float run_until_silent(int max_blocks = 200)
  {
    float peak = 0.f;
    for(int i = 0; i < max_blocks; i++)
    {
      const float v = tick();
      peak = v > peak ? v : peak;
      if(i > 0 && v == 0.f)
        break;
    }
    return peak;
  }
};

template <typename F>
void with_adsr(F&& f)
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto* proc = score::test::add_process(*doc, adsr_uuid, {});
    if(!proc)
      SKIP("ao::ADSR is not built");
    REQUIRE(proc->inlets().size() == 6);
    CHECK(proc->inlets()[hold_inlet]->name() == "Hold");
    CHECK(proc->inlets()[trig_inlet]->name() == "Trigger");

    auto& plug = load_execution(*doc);
    auto comp = component(plug, *proc);
    auto& node = static_cast<oscr::safe_node<ao::ADSR>&>(*comp->node);

    adsr_exec e{*doc, *proc, plug, comp, node};
    f(e);

    spin(50);
  });
}
}

TEST_CASE("ADSR: the inspector bang reaches the object without the port", "[avnd][adsr][execution]")
{
  with_adsr([](adsr_exec& e) {
    e.fast();

    // What ControlWidgets::ImpulseButton does on QPushButton::pressed: it
    // emits the model inlet's signal, it does not setValue()
    control(e.proc, trig_inlet)->valueChanged(ossia::impulse{});
    run_exec(e.plug);

    CHECK(bool(e.object().inputs.trig));
    // Nothing was written into the execution port
    auto* in = static_cast<ossia::value_inlet*>(e.node.root_inputs()[trig_inlet]);
    CHECK(in->data.get_data().empty());

    const float first = e.tick();
    CHECK(first > 0.f);
    // finish_run() consumed the impulse
    CHECK(!e.object().inputs.trig);

    const float peak = e.run_until_silent();
    CHECK(peak == Approx(1.f).margin(0.01));
    CHECK(e.object().outputs.out.value == 0.f);
  });
}

TEST_CASE("ADSR: a bang and a cable value give the same envelope", "[avnd][adsr][execution]")
{
  with_adsr([](adsr_exec& e) {
    e.fast();

    control(e.proc, trig_inlet)->valueChanged(ossia::impulse{});
    run_exec(e.plug);
    const float from_ui = e.tick();
    e.run_until_silent();

    // What a cable or an OSC message does instead: a value in the port
    auto* in = static_cast<ossia::value_inlet*>(e.node.root_inputs()[trig_inlet]);
    in->data.write_value(ossia::impulse{}, 0);
    const float from_port = e.tick();
    in->data.clear();
    e.run_until_silent();

    CHECK(from_ui == Approx(from_port));
  });
}

TEST_CASE("ADSR: the inspector Hold gates the envelope", "[avnd][adsr][execution]")
{
  with_adsr([](adsr_exec& e) {
    e.fast();
    control(e.proc, 4)->setValue(0.25f); // Sustain
    run_exec(e.plug);

    // ControlWidgets::Button in the inspector: setValue(true) on press,
    // setValue(false) on release. No command, no port write.
    control(e.proc, hold_inlet)->setValue(true);
    run_exec(e.plug);
    CHECK(e.object().inputs.hold == true);
    auto* in = static_cast<ossia::value_inlet*>(e.node.root_inputs()[hold_inlet]);
    CHECK(in->data.get_data().empty());

    float last = 0.f;
    for(int i = 0; i < 30; i++)
      last = e.tick();
    CHECK(last == Approx(0.25f).margin(0.001));
    for(int i = 0; i < 20; i++)
      CHECK(e.tick() == Approx(0.25f).margin(0.001));

    control(e.proc, hold_inlet)->setValue(false);
    run_exec(e.plug);
    CHECK(e.object().inputs.hold == false);

    for(int i = 0; i < 40; i++)
    {
      last = e.tick();
      if(last == 0.f)
        break;
    }
    CHECK(last == 0.f);
  });
}

TEST_CASE("ADSR: a bang while the node is not ticked stays pending", "[avnd][adsr][execution]")
{
  with_adsr([](adsr_exec& e) {
    e.fast();

    // Nothing clears the field but a tick: only the node's own finish_run()
    // resets the optional, and it is a tick that calls it. Several bangs
    // before the next tick collapse into one.
    control(e.proc, trig_inlet)->valueChanged(ossia::impulse{});
    control(e.proc, trig_inlet)->valueChanged(ossia::impulse{});
    control(e.proc, trig_inlet)->valueChanged(ossia::impulse{});
    run_exec(e.plug);
    CHECK(bool(e.object().inputs.trig));

    CHECK(e.tick() > 0.f);
    CHECK(!e.object().inputs.trig);
    const float peak = e.run_until_silent();
    CHECK(peak == Approx(1.f).margin(0.01));
  });
}

TEST_CASE("ADSR: a press and release between two ticks is lost either way", "[avnd][adsr][execution]")
{
  with_adsr([](adsr_exec& e) {
    e.fast();

    // The UI writes the field in place, the port keeps only the last value of
    // the tick: neither path can carry a gate that opens and closes without a
    // tick in between.
    control(e.proc, hold_inlet)->setValue(true);
    control(e.proc, hold_inlet)->setValue(false);
    run_exec(e.plug);
    CHECK(e.object().inputs.hold == false);
    for(int i = 0; i < 10; i++)
      CHECK(e.tick() == 0.f);

    auto* in = static_cast<ossia::value_inlet*>(e.node.root_inputs()[hold_inlet]);
    in->data.write_value(true, 3);
    in->data.write_value(false, 29);
    CHECK(e.tick() == 0.f);
    in->data.clear();
    CHECK(e.object().inputs.hold == false);
    for(int i = 0; i < 10; i++)
      CHECK(e.tick() == 0.f);
  });
}

TEST_CASE("ADSR: Hold set before the transport starts is not an edge", "[avnd][adsr][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto* proc = score::test::add_process(*doc, adsr_uuid, {});
    if(!proc)
      SKIP("ao::ADSR is not built");

    // The model already has the gate down when the executor builds the node:
    // the object's prepare() runs after the control values are pushed.
    control(*proc, hold_inlet)->setValue(true);
    control(*proc, attack_inlet)->setValue(0.01f);
    control(*proc, decay_inlet)->setValue(0.01f);

    auto& plug = load_execution(*doc);
    auto comp = component(plug, *proc);
    auto& node = static_cast<oscr::safe_node<ao::ADSR>&>(*comp->node);
    adsr_exec e{*doc, *proc, plug, comp, node};

    CHECK(e.object().inputs.hold == true);
    for(int i = 0; i < 30; i++)
      CHECK(e.tick() == 0.f);

    spin(50);
  });
}
