// Control utilities (Enumerator, Counter, Value Delay) driven like in score,
// through the ossia binding: from a cable or an address (a value in the
// execution port) and from the inspector (the model inlet, applied from the
// execution queue). The unit tests of Utilities_Test.cpp call the objects
// directly and miss what the binding does around them.

#include <Process/Dataflow/WidgetInlets.hpp>
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
#include <catch2/catch_test_macros.hpp>
#include <examples/Advanced/Utilities/Counter.hpp>
#include <examples/Advanced/Utilities/Enumerator.hpp>
#include <examples/Helpers/ValueDelay.hpp>
#include <examples/Advanced/Utilities/Accumulator.hpp>
#include <examples/Advanced/Utilities/ArrayBest.hpp>
#include <examples/Advanced/Utilities/ArrayRecombiner.hpp>
#include <examples/Advanced/Utilities/Spigot.hpp>
#include <AvndProcesses/Queue.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

#include <ossia/network/value/format_value.hpp>

template <>
struct Catch::StringMaker<ossia::value>
{
  static std::string convert(const ossia::value& v) { return fmt::format("{}", v); }
};

namespace
{
using score::test::run_exec;

template <typename T>
struct exec
{
  Process::ProcessModel& proc;
  Execution::DocumentPlugin& plug;
  std::shared_ptr<Execution::ProcessComponent> comp;
  oscr::safe_node<T>& node;
  int64_t date{};

  T& object() { return node.impl.effect; }

  Process::ControlInlet& control(int i)
  {
    auto* c = qobject_cast<Process::ControlInlet*>(proc.inlets()[i]);
    REQUIRE(c);
    return *c;
  }

  //! The inspector: the model inlet; applied from the execution queue.
  void gui(int i, const ossia::value& v)
  {
    control(i).setValue(v);
    run_exec(plug);
  }
  //! Picks a choice by name in the inspector's combo box: a Process::Enum
  //! holds the names themselves, a Process::ComboBox the values they stand for.
  void gui_choice(int i, const QString& name)
  {
    auto& ctl = control(i);
    if(auto* e = qobject_cast<Process::Enum*>(&ctl))
    {
      REQUIRE(ossia::contains(e->getValues(), name));
      gui(i, name.toStdString());
    }
    else if(auto* c = qobject_cast<Process::ComboBox*>(&ctl))
    {
      auto it = ossia::find_if(c->getValues(), [&](auto& p) { return p.first == name; });
      REQUIRE(it != c->getValues().end());
      gui(i, it->second);
    }
    else
    {
      FAIL("not a combo box: " << ctl.name().toStdString());
    }
  }
  void gui_bang(int i)
  {
    // ControlWidgets::ImpulseButton emits the signal, it does not setValue()
    control(i).valueChanged(ossia::impulse{});
    run_exec(plug);
  }

  //! A cable, or an address on the port: a value in the execution port.
  void port(int i, const ossia::value& v)
  {
    auto* in = static_cast<ossia::value_inlet*>(node.root_inputs()[i]);
    (*in)->write_value(v, 0);
  }

  //! Runs one tick; returns the last value of outlet `o`, if any.
  std::optional<ossia::value> tick(int o = 0, int frames = 64)
  {
    // Dates are in flicks: a tick of 0 samples does not run the object
    const double rate = plug.context().execState->sampleRate;
    const auto to_flicks = [&](int64_t samples) {
      return ossia::time_value{int64_t(samples * ossia::flicks_per_second<double> / rate)};
    };
    ossia::token_request tk;
    tk.prev_date = to_flicks(date);
    tk.date = to_flicks(date + frames);
    tk.start_sample = 0;
    tk.length_sample = frames;
    date += frames;

    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    node.run(tk, ossia::exec_state_facade{plug.context().execState.get()});
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

    for(auto* in : node.root_inputs())
      if(auto* vi = in->template target<ossia::value_port>())
        vi->clear();

    std::optional<ossia::value> res;
    auto* out = node.root_outputs()[o]->template target<ossia::value_port>();
    REQUIRE(out);
    if(!out->get_data().empty())
      res = out->get_data().back().value;
    for(auto* outlet : node.root_outputs())
      if(auto* vo = outlet->template target<ossia::value_port>())
        vo->clear();
    return res;
  }
};

template <typename T, typename F>
void with(const QString& uuid, F&& f)
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, uuid, {});
    if(!proc)
      SKIP("not built");

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    run_exec(plug);
    REQUIRE(plug.baseScenario());
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto it = procs.find(proc->id());
    REQUIRE(it != procs.end());
    REQUIRE(it->second->node);
    auto comp = it->second;
    exec<T> e{*proc, plug, comp, static_cast<oscr::safe_node<T>&>(*comp->node)};
    f(e);
    QApplication::processEvents();
  });
}

const ossia::value bang{ossia::impulse{}};
const QString enumerator_uuid = QStringLiteral("7e998d33-864d-4483-83f4-a9db48e5703f");
const QString counter_uuid = QStringLiteral("acdc0a7e-676f-462c-b46d-c6cd99fa74a2");
const QString value_delay_uuid = QStringLiteral("39a7a489-a86b-4eaa-a617-ec2c9d559744");
const QString spigot_uuid = QStringLiteral("8b75d69b-5ce4-4360-a066-c4a7f37f3353");
const QString array_best_uuid = QStringLiteral("9e793245-72a1-4aa2-a813-5f5836cf1637");
const QString recombiner_uuid = QStringLiteral("8a833254-04ef-42f0-bd39-a8a3b8ce94c3");
const QString accumulator_uuid = QStringLiteral("5c5b37b5-da06-432a-bc51-81657b6d59e1");
const QString queue_uuid = QStringLiteral("8f68b81e-e5ba-4a10-a888-6581a5d770fe");

//! "A | B | C": the names of the inlets, to check the indices used.
template <typename E>
std::string inlet_names(E& e)
{
  std::string names;
  for(auto* i : e.proc.inlets())
    names += (names.empty() ? "" : " | ") + i->name().toStdString();
  return names;
}
}

// Enumerator inlets: Trigger, List, Mode, Bounds, Interval
TEST_CASE("Enumerator through the binding: a list, then bangs", "[avnd][enumerator][execution]")
{
  with<ao::Enumerator>(enumerator_uuid, [](auto& e) {
    REQUIRE(e.proc.inlets().size() == 5);
    REQUIRE(e.proc.inlets()[0]->name() == "Trigger");
    REQUIRE(e.proc.inlets()[1]->name() == "List");
    const auto list = ossia::value{std::vector<ossia::value>{1, 3, 4, 5, 12, 14}};

    // The list arrives once (Object Filter sends it when it changes)
    e.port(1, list);
    CHECK_FALSE(e.tick());

    e.port(0, bang);
    auto v = e.tick();
    REQUIRE(v);
    CHECK(*v == ossia::value{1});

    // The list is not re-sent: the next bang still walks it
    e.port(0, bang);
    v = e.tick();
    REQUIRE(v);
    CHECK(*v == ossia::value{3});

    e.port(0, ossia::value{4});
    v = e.tick();
    REQUIRE(v);
    CHECK(*v == ossia::value{12});
  });
}

TEST_CASE("Enumerator through the binding: automatic modes from the inspector", "[avnd][enumerator][execution]")
{
  with<ao::Enumerator>(enumerator_uuid, [](auto& e) {
    e.port(1, ossia::value{std::vector<ossia::value>{1, 3, 4}});
    e.tick();
    e.gui_choice(2, "EveryTick"); // Mode
    CHECK(e.object().inputs.mode.value == ao::Enumerator::EveryTick);
    auto v = e.tick();
    REQUIRE(v);
    CHECK(*v == ossia::value{1});
    v = e.tick();
    REQUIRE(v);
    CHECK(*v == ossia::value{3});
  });
}

// Counter inlets: Mode, Max, Output, Reset, Send; messages: Increase
TEST_CASE("Counter through the binding: Output in Manually mode", "[avnd][counter][execution]")
{
  with<examples::Counter>(counter_uuid, [](auto& e) {
    REQUIRE(e.proc.inlets()[3]->name() == "Output");
    REQUIRE(e.proc.inlets()[4]->name() == "Reset");
    e.gui_choice(5, "Manually");
    REQUIRE(e.object().inputs.when.value == examples::Counter::Manually);
    e.tick();
    e.object().count = 5;
    // Nothing goes out by itself
    CHECK_FALSE(e.tick());

    SECTION("inspector button")
    {
      e.gui_bang(3);
      auto v = e.tick();
      REQUIRE(v);
      CHECK(*v == ossia::value{5});
      CHECK_FALSE(e.tick());
    }
    SECTION("cable / address")
    {
      e.port(3, bang);
      auto v = e.tick();
      REQUIRE(v);
      CHECK(*v == ossia::value{5});
      CHECK_FALSE(e.tick());
    }
    SECTION("Reset then Output")
    {
      e.gui_bang(4);
      CHECK_FALSE(e.tick()); // Manually: reset does not send
      e.gui_bang(3);
      auto v = e.tick();
      REQUIRE(v);
      CHECK(*v == ossia::value{0});
    }
  });
}

// Value Delay inlets: In, Length, Count, Mode, Time
TEST_CASE("Value Delay through the binding: the Time control", "[avnd][value_delay][execution]")
{
  with<examples::helpers::ValueDelay>(value_delay_uuid, [](auto& e) {
    REQUIRE(e.proc.inlets().size() == 10);
    CHECK(e.proc.inlets()[4]->name() == "Time");
    // The inspector's time chooser: {time, mode}, mode 0 is seconds
    e.gui(4, ossia::value{ossia::vec2f{0.25f, 0.f}});
    CHECK(e.object().inputs.time.value == 0.25f);
    // A cable or an OSC message: a plain number is seconds too
    e.port(4, ossia::value{0.5f});
    e.tick();
    CHECK(e.object().inputs.time.value == 0.5f);
    e.port(4, ossia::value{2});
    e.tick();
    CHECK(e.object().inputs.time.value == 2.f);
  });
}

// Value Delay inlets: In, Length, Count, Mode, Time, Feedback, Mix, Freeze,
// Clear, Smooth. Outlets: Out, Mix.
TEST_CASE("Value Delay through the binding: any value, and the Mix outlet", "[avnd][value_delay][execution]")
{
  with<examples::helpers::ValueDelay>(value_delay_uuid, [](auto& e) {
    REQUIRE(inlet_names(e) == "In | Length | Count | Mode | Time | Feedback | Mix | Freeze | Clear | Smooth");
    e.gui(1, 1); // Length
    e.gui(2, 1); // Count
    e.gui(3, 1); // Messages
    e.gui(6, 0.f); // Mix: In only

    e.port(0, ossia::value{std::string{"a"}});
    e.tick();
    e.port(0, ossia::value{ossia::vec3f{1.f, 2.f, 3.f}});
    auto out = e.tick(0);
    REQUIRE(out);
    CHECK(*out == ossia::value{std::vector<ossia::value>{std::string{"a"}}});

    // A tick without message: the vec3 is what In is, on the Mix outlet
    auto mix = e.tick(1);
    REQUIRE(mix);
    CHECK(*mix == ossia::value{ossia::vec3f{1.f, 2.f, 3.f}});

    // Clear from the inspector: the tap reads In
    e.gui_bang(8);
    out = e.tick(0);
    REQUIRE(out);
    CHECK(*out == ossia::value{std::vector<ossia::value>{ossia::vec3f{1.f, 2.f, 3.f}}});
  });
}

TEST_CASE("Spigot through the binding: Enabled from the inspector and from a cable", "[avnd][spigot][execution]")
{
  with<ao::Spigot>(spigot_uuid, [](auto& e) {
    REQUIRE(inlet_names(e) == "Input | Enabled");
    e.port(0, ossia::value{1.5f});
    CHECK_FALSE(e.tick()); // disabled by default

    e.gui(1, ossia::value{true});
    e.port(0, ossia::value{2.5f});
    auto v = e.tick();
    REQUIRE(v);
    CHECK(*v == ossia::value{2.5f});
    CHECK_FALSE(e.tick()); // only what arrives

    // A cable or an OSC message with a number: 0 disables, 1 enables
    e.port(1, ossia::value{0});
    e.port(0, ossia::value{3.f});
    CHECK_FALSE(e.tick());
    e.port(1, ossia::value{1});
    e.port(0, ossia::value{4.f});
    CHECK(e.tick() == ossia::value{4.f});
    e.port(1, ossia::value{0.f});
    e.port(0, ossia::value{5.f});
    CHECK_FALSE(e.tick());
  });
}

TEST_CASE("Array recombiner through the binding: lists and vectors", "[avnd][recombiner][execution]")
{
  with<ao::ArrayRecombiner>(recombiner_uuid, [](auto& e) {
    INFO(inlet_names(e));
    REQUIRE(e.proc.inlets().size() == 3);
    e.gui(1, ossia::value{2});
    e.port(0, ossia::value{std::vector<ossia::value>{1, 2, 3, 4, 5, 6}});
    auto v = e.tick();
    REQUIRE(v);
    CHECK(*v == ossia::value{std::vector<ossia::value>{
               std::vector<ossia::value>{1, 2}, std::vector<ossia::value>{3, 4},
               std::vector<ossia::value>{5, 6}}});
    // Nothing new: nothing out
    CHECK_FALSE(e.tick());
    e.port(0, ossia::value{ossia::vec4f{1.f, 2.f, 3.f, 4.f}});
    v = e.tick();
    REQUIRE(v);
    CHECK(*v == ossia::value{std::vector<ossia::value>{
               std::vector<ossia::value>{1.f, 2.f}, std::vector<ossia::value>{3.f, 4.f}}});
  });
}

TEST_CASE("Accumulator through the binding: Output in Manually mode", "[avnd][accumulator][execution]")
{
  with<ao::Accumulator>(accumulator_uuid, [](auto& e) {
    REQUIRE(inlet_names(e) == "In | Reset | Output | Send");
    e.gui_choice(3, "Manually");
    REQUIRE(e.object().inputs.when.value == ao::Accumulator::Manually);
    for(float f : {1.f, 2.f, 3.f})
    {
      e.port(0, ossia::value{f});
      CHECK_FALSE(e.tick(0)); // Sum: nothing until the bang
    }
    SECTION("inspector")
    {
      e.gui_bang(2);
      CHECK(e.tick(0) == ossia::value{6.f});
    }
    SECTION("cable / OSC")
    {
      e.port(2, bang);
      CHECK(e.tick(0) == ossia::value{6.f});
    }
    CHECK_FALSE(e.tick(0));
  });
}

TEST_CASE("Buffer queue through the binding: Bang and Clear", "[avnd][queue][execution]")
{
  with<avnd_tools::Queue>(queue_uuid, [](auto& e) {
    REQUIRE(inlet_names(e) == "Input | Max length | Clear | Lock | Mode | Data | Bang");
    for(int i : {1, 2, 3})
    {
      e.port(0, ossia::value{i});
      e.tick();
    }
    SECTION("Bang from the inspector and from a cable")
    {
      e.gui_bang(6);
      CHECK(e.tick());
      e.port(6, bang);
      CHECK(e.tick());
    }
    SECTION("Clear: an impulse clears once")
    {
      e.port(2, bang);
      e.tick();
      CHECK_FALSE(e.object().inputs.clear.value); // released after the tick
      e.port(0, ossia::value{7});
      e.tick();
      e.gui_bang(6);
      auto v = e.tick();
      REQUIRE(v);
      INFO(fmt::format("{}", *v));
      // Only what came after the clear
      CHECK(ossia::convert<std::string>(*v).find('1') == std::string::npos);
    }
  });
}

TEST_CASE("Array best through the binding: sends when an array arrives", "[avnd][array-best][execution]")
{
  with<ao::ArrayBest>(array_best_uuid, [](auto& e) {
    INFO(inlet_names(e));
    e.port(0, ossia::value{std::vector<ossia::value>{0.1f, 0.7f, 0.2f}});
    CHECK(e.tick(0) == ossia::value{1}); // Index
    // Nothing new: nothing out, instead of the same index at every tick
    CHECK_FALSE(e.tick(0));
    // A control change recomputes with the last array
    e.gui_choice(1, "Lowest");
    CHECK(e.tick(0) == ossia::value{0});
  });
}

TEST_CASE("Impulse buttons are not pressed by starting the execution", "[avnd][impulse][execution]")
{
  with<ao::Accumulator>(accumulator_uuid, [](auto& e) {
    CHECK_FALSE(e.object().bang);
  });
  with<avnd_tools::Queue>(queue_uuid, [](auto& e) {
    CHECK_FALSE(e.object().banged);
  });
}
