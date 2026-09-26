// Processes whose ports follow the value of one of their controls
#include <State/Address.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessState.hpp>
#include <Process/State/MessageNode.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Commands/Cohesion/InterpolateStates.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Automation/AutomationModel.hpp>
#include <JS/Qml/EditContext.hpp>
#include <Crousti/Executor.hpp>
#include <Crousti/ProcessModel.hpp>
#include <examples/Advanced/AI/PromptComposer.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <LocalTree/ScriptableProcessComponent.hpp>
#include <Scenario/Commands/SetControllerControlValue.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Commands/State/SnapshotProcess.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>


#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>
#include <score_test/Scriptable.hpp>

#include <catch2/catch_all.hpp>

#include <thread>

namespace
{
const QString composer_uuid = QStringLiteral("a4227e94-cf7d-4776-9aa0-2f384be7d97f");
const QString value_mixer_uuid = QStringLiteral("0fd108dd-daa8-4667-869d-f409da70e823");

using score::test::find_control;
using score::test::find_published;
using score::test::launch_state;
using score::test::messages;
using score::test::process_events;
using score::test::push_from_thread;
using score::test::wait_past_burst;
using score::test::wait_until;

ossia::value rows(std::vector<std::pair<int, std::string>> r)
{
  std::vector<ossia::value> res;
  for(auto& [k, t] : r)
    res.push_back(std::vector<ossia::value>{k, t});
  return res;
}

Process::ControlInlet* keywords(Process::ProcessModel& p)
{
  return qobject_cast<Process::ControlInlet*>(p.inlets()[0]);
}

std::vector<Process::ControlInlet*> knobs(Process::ProcessModel& p)
{
  std::vector<Process::ControlInlet*> res;
  for(auto in : p.inlets())
    if(in->id().val() >= 12000)
      if(auto c = qobject_cast<Process::ControlInlet*>(in))
        res.push_back(c);
  return res;
}

float valueOf(Process::ProcessModel& p, const QString& name)
{
  auto c = find_control(p, name);
  REQUIRE(c);
  return ossia::convert<float>(c->value());
}


struct Composer
{
  score::Document* doc{};
  Process::ProcessModel* proc{};
  explicit Composer(const score::GUIApplicationContext& app)
  {
    doc = score::test::new_document(app);
    proc = score::test::add_process(*doc, composer_uuid, {});
    REQUIRE(proc);
    CommandDispatcher<> disp{ctx().commandStack};
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("pc"));
    disp.submit<Process::SetProcessScriptable>(*proc, true);
    setRows(rows({{12000, "a"}, {12001, "b"}, {12002, "c"}}));
    auto k = knobs(*proc);
    REQUIRE(k.size() == 3);
    for(int i = 0; i < 3; i++)
      disp.submit<Process::SetValue>(*k[i], ossia::value{0.1f * (i + 1)});
    process_events();
  }
  const score::DocumentContext& ctx() const { return doc->context(); }
  void setRows(const ossia::value& v)
  {
    CommandDispatcher<>{ctx().commandStack}.submit<Scenario::SetControllerControlValue>(
        *keywords(*proc), v, ctx());
    process_events();
  }
  State::Address address(const QString& name)
  {
    return State::Address{QStringLiteral("score"), {"controls", "pc", name}};
  }
};
}



TEST_CASE(
    "keywords written through the namespace are an edit that undo reverts with the removed knobs",
    "[integration][scriptable][dynamic]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    Composer f{app};
    auto& stack = f.doc->commandStack();
    const int before = stack.currentIndex();

    auto kw = find_published(f.ctx(), LocalTree::scriptableAddress(*keywords(*f.proc)));
    REQUIRE(kw);
    push_from_thread(*kw, rows({{12000, "a"}}));
    REQUIRE(knobs(*f.proc).size() == 1);
    REQUIRE(wait_until([&] { return stack.currentIndex() == before + 1; }));

    stack.undo();
    process_events();
    REQUIRE(knobs(*f.proc).size() == 3);
    CHECK(valueOf(*f.proc, "b") == Catch::Approx(0.2f));
    CHECK(valueOf(*f.proc, "c") == Catch::Approx(0.3f));
    CHECK(LocalTree::scriptableAddress(*find_control(*f.proc, "c")) == f.address("c"));
  });
}

TEST_CASE(
    "undoing the removal of a knob gives it back its value, not what its hidden node received",
    "[integration][scriptable][dynamic]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    Composer f{app};
    auto& stack = f.doc->commandStack();
    f.setRows(rows({{12000, "a"}}));
    REQUIRE(knobs(*f.proc).size() == 1);

    // The node of the removed knob "c" stays, hidden, and receives a value
    auto hidden = find_published(f.ctx(), f.address("c"));
    REQUIRE(hidden);
    push_from_thread(*hidden, 0.9f);

    stack.undo();
    process_events();
    REQUIRE(knobs(*f.proc).size() == 3);
    CHECK(valueOf(*f.proc, "c") == Catch::Approx(0.3f));

    // A new edit that brings the knob back takes the written value
    stack.redo();
    process_events();
    push_from_thread(*find_published(f.ctx(), f.address("c")), 0.9f);
    f.setRows(rows({{12000, "a"}, {12002, "c"}}));
    CHECK(valueOf(*f.proc, "c") == Catch::Approx(0.9f));
  });
}

TEST_CASE(
    "a state sets knobs that the process gets from the same state",
    "[integration][scriptable][dynamic]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    Composer f{app};
    auto& cue = score::test::start_state(*f.doc);
    Scenario::Command::snapshotProcessInState(cue, *f.proc, f.ctx());
    process_events();
    REQUIRE(messages(cue).size() == 4);

    f.setRows(rows({{12000, "a"}}));
    REQUIRE(knobs(*f.proc).size() == 1);
    CHECK(!f.ctx().plugin<LocalTree::DocumentPlugin>().references().broken().empty());

    launch_state(f.ctx(), cue);
    REQUIRE(wait_until([&] { return knobs(*f.proc).size() == 3; }));
    CHECK(valueOf(*f.proc, "a") == Catch::Approx(0.1f));
    CHECK(valueOf(*f.proc, "b") == Catch::Approx(0.2f));
    CHECK(valueOf(*f.proc, "c") == Catch::Approx(0.3f));
    CHECK(f.ctx().plugin<LocalTree::DocumentPlugin>().references().broken().empty());
  });
}

TEST_CASE(
    "a state sets a knob that the process never had in this session",
    "[integration][scriptable][dynamic]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    Composer f{app};
    auto& cue = score::test::start_state(*f.doc);
    // As a document saved with a row "d" and opened without it
    CommandDispatcher<>{f.ctx().commandStack}.submit<Scenario::Command::AddMessagesToState>(
        cue, State::MessageList{
                 {State::AddressAccessor{f.address("d")}, 0.7f},
                 {State::AddressAccessor{LocalTree::scriptableAddress(*keywords(*f.proc))},
                  rows({{12000, "a"}, {12003, "d"}})}});
    process_events();

    launch_state(f.ctx(), cue);
    REQUIRE(wait_until([&] { return knobs(*f.proc).size() == 2; }));
    CHECK(valueOf(*f.proc, "d") == Catch::Approx(0.7f));
    CHECK(LocalTree::scriptableAddress(*find_control(*f.proc, "d")) == f.address("d"));
  });
}

TEST_CASE(
    "keywords written while playing change the ports of the document",
    "[integration][scriptable][dynamic]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    Composer f{app};
    auto& stack = f.doc->commandStack();
    const int before = stack.currentIndex();

    auto& plug = f.ctx().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*f.doc));
    score::test::run_exec(plug);

    auto kw = find_published(f.ctx(), LocalTree::scriptableAddress(*keywords(*f.proc)));
    REQUIRE(kw);
    push_from_thread(*kw, rows({{12000, "a"}, {12001, "b"}, {12002, "c"}, {12003, "d"}}));
    REQUIRE(knobs(*f.proc).size() == 4);
    REQUIRE(wait_until([&] { return stack.currentIndex() == before + 1; }));

    score::test::run_exec(plug);
    plug.clear();
    process_events();
    REQUIRE(knobs(*f.proc).size() == 4);
  });
}

TEST_CASE(
    "a state restores the controls of ports that follow a count",
    "[integration][scriptable][dynamic]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto mixer = score::test::add_process(*doc, value_mixer_uuid, {});
    if(!mixer)
      SKIP("ao::ValueMixer is not built");
    CommandDispatcher<> disp{ctx.commandStack};
    disp.submit<Process::RenameProcess>(*mixer, QStringLiteral("mixer"));
    disp.submit<Process::SetProcessScriptable>(*mixer, true);
    auto count = qobject_cast<Process::ControlInlet*>(mixer->inlets()[0]);
    REQUIRE(count);
    REQUIRE(count->changesPorts);
    disp.submit<Scenario::SetControllerControlValue>(*count, ossia::value{3}, ctx);
    process_events();
    disp.submit<Process::SetValue>(*find_control(*mixer, "Mix 2"), ossia::value{0.25f});
    process_events();

    auto& cue = score::test::start_state(*doc);
    Scenario::Command::snapshotProcessInState(cue, *mixer, ctx);
    process_events();

    disp.submit<Scenario::SetControllerControlValue>(*count, ossia::value{1}, ctx);
    process_events();
    REQUIRE(!find_control(*mixer, "Mix 2"));

    launch_state(ctx, cue);
    REQUIRE(wait_until([&] { return find_control(*mixer, "Mix 2") != nullptr; }));
    CHECK(valueOf(*mixer, "Mix 2") == Catch::Approx(0.25f));
  });
}

namespace
{
void runExec(Execution::DocumentPlugin& plug)
{
  score::test::run_exec(plug);
}

Execution::DocumentPlugin& startExecution(score::Document& doc)
{
  auto& plug = doc.context().plugin<Execution::DocumentPlugin>();
  plug.reload(true, score::test::base_interval(doc));
  runExec(plug);
  return plug;
}

ai::PromptComposer& executed(Execution::DocumentPlugin& plug, const Process::ProcessModel& p)
{
  REQUIRE(plug.baseScenario());
  auto& procs = plug.baseScenario()->baseInterval().processes();
  auto it = procs.find(p.id());
  REQUIRE(it != procs.end());
  return static_cast<oscr::safe_node<ai::PromptComposer>&>(*it->second->node).impl.effect;
}
}

TEST_CASE(
    "knobs a state brings back while playing reach the execution with their values",
    "[integration][scriptable][dynamic]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    Composer f{app};
    auto& cue = score::test::start_state(*f.doc);
    Scenario::Command::snapshotProcessInState(cue, *f.proc, f.ctx());
    process_events();
    f.setRows(rows({{12000, "a"}}));

    auto& plug = startExecution(*f.doc);
    {
      auto s = Engine::score_to_ossia::state(cue, *plug.context().execState);
      std::thread{[&] { s.launch(); }}.join();
    }
    REQUIRE(wait_until([&] { return knobs(*f.proc).size() == 3; }));
    runExec(plug);
    runExec(plug);

    auto& object = executed(plug, *f.proc);
    object();
    CHECK(object.outputs.out.value == "(a:0.1), (b:0.2), (c:0.3)");
    plug.clear();
    process_events();
  });
}

TEST_CASE(
    "a process removed and restored while playing publishes its knobs under their names",
    "[integration][scriptable][dynamic]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    Composer f{app};
    f.setRows(rows({{12000, "a"}}));
    auto& plug = startExecution(*f.doc);

    auto& itv = score::test::base_interval(*f.doc);
    const auto id = f.proc->id();
    CommandDispatcher<>{f.ctx().commandStack}.submit(
        new Scenario::Command::RemoveProcessFromInterval{itv, id});
    process_events();
    runExec(plug);
    f.doc->commandStack().undo();
    process_events();
    runExec(plug);
    f.proc = &itv.processes.at(id);
    f.setRows(rows({{12000, "a"}, {12001, "b"}, {12002, "c"}}));

    auto b = find_control(*f.proc, "b");
    REQUIRE(b);
    CHECK(b->exposed() == QStringLiteral("b"));
    CHECK(LocalTree::scriptableAddress(*b) == f.address("b"));
    plug.clear();
    process_events();
  });
}

TEST_CASE(
    "writes to a control that changes ports are one edit, and values of another type are ignored",
    "[integration][scriptable][dynamic]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    Composer f{app};
    auto& stack = f.doc->commandStack();
    const int before = stack.currentIndex();
    auto kw = find_published(f.ctx(), LocalTree::scriptableAddress(*keywords(*f.proc)));
    REQUIRE(kw);

    std::thread{[&] {
      kw->push_value(rows({{12000, "a"}, {12001, "b"}}));
      kw->push_value(rows({{12000, "a"}}));
      kw->push_value(rows({{12000, "a"}, {12003, "d"}}));
    }}.join();
    process_events();
    // Applied at once, committed as one edit once the writes stop
    REQUIRE(knobs(*f.proc).size() == 2);
    CHECK(!stack.isAtSavedIndex());
    REQUIRE(wait_until([&] { return stack.currentIndex() == before + 1; }));

    stack.undo();
    process_events();
    REQUIRE(knobs(*f.proc).size() == 3);
    CHECK(valueOf(*f.proc, "c") == Catch::Approx(0.3f));

    // A float, e.g. from an automation, into the list of rows
    push_from_thread(*kw, ossia::value{0.5f});
    wait_past_burst();
    CHECK(knobs(*f.proc).size() == 3);
    CHECK(stack.currentIndex() == before);
  });
}

TEST_CASE(
    "interpolating states skips controls that change ports",
    "[integration][scriptable][dynamic]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    Composer f{app};
    auto& scenar = score::test::base_scenario(*f.doc);
    auto create = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        scenar, scenar.states.begin()->id(), TimeVal::fromMsecs(2000), 0.3, false};
    const auto itvId = create->createdInterval();
    CommandDispatcher<>{f.ctx().commandStack}.submit(create);
    auto& itv = scenar.interval(itvId);
    auto& start = scenar.state(itv.startState());
    auto& end = scenar.state(itv.endState());

    Scenario::Command::snapshotProcessInState(start, *f.proc, f.ctx());
    process_events();
    f.setRows(rows({{12000, "a"}, {12001, "b"}}));
    CommandDispatcher<>{f.ctx().commandStack}.submit<Process::SetValue>(
        *find_control(*f.proc, "a"), ossia::value{0.9f});
    Scenario::Command::snapshotProcessInState(end, *f.proc, f.ctx());
    process_events();

    Scenario::Command::InterpolateStates({&itv}, f.ctx().commandStack);
    process_events();
    const auto kwAddr = LocalTree::scriptableAddress(*keywords(*f.proc));
    bool knob = false;
    for(auto& p : itv.processes)
      if(auto autom = qobject_cast<Automation::ProcessModel*>(&p))
      {
        CHECK(autom->address().address != kwAddr);
        if(autom->address().address == f.address("a"))
          knob = true;
      }
    CHECK(knob);
  });
}

TEST_CASE(
    "a pattern in a state does not create an address for a missing port",
    "[integration][scriptable][dynamic]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    Composer f{app};
    auto& cue = score::test::start_state(*f.doc);
    CommandDispatcher<>{f.ctx().commandStack}.submit<Scenario::Command::AddMessagesToState>(
        cue, State::MessageList{{*State::parseAddressAccessor("score:/controls/pc/*"), 0.7f}});
    process_events();
    launch_state(f.ctx(), cue);
    process_events();
    auto& dev = f.ctx().plugin<LocalTree::DocumentPlugin>().device();
    auto pc = ossia::net::find_node(dev.get_root_node(), "/controls/pc");
    REQUIRE(pc);
    for(auto child : pc->children_copy())
      CHECK(child->get_name().find('*') == std::string::npos);
  });
}

TEST_CASE(
    "a script setting a control that changes ports keeps the removed ports for undo",
    "[integration][scriptable][dynamic]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    Composer f{app};
    JS::EditJsContext js;
    js.setValue(keywords(*f.proc), QList<QVariant>{QVariantList{12000, "a"}});
    process_events();
    REQUIRE(knobs(*f.proc).size() == 1);
    f.doc->commandStack().undo();
    process_events();
    REQUIRE(knobs(*f.proc).size() == 3);
    CHECK(valueOf(*f.proc, "b") == Catch::Approx(0.2f));
  });
}

namespace
{
// score-addon-onnx's Regressor: a "Param. count" spinbox sizes a dynamic
// group of knobs through on_controller_interaction.
const QString regressor_uuid = QStringLiteral("c9613fba-6318-463c-91b0-cab4c6c7ab2b");

int paramKnobs(Process::ProcessModel& p)
{
  int n = 0;
  for(auto in : p.inlets())
    if(in->name().startsWith(QStringLiteral("Param. ")) && in->name() != QStringLiteral("Param. count"))
      n++;
  return n;
}
}

TEST_CASE(
    "the regressor's parameter count adds and removes its knobs",
    "[integration][dynamic][regressor]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto proc = score::test::add_process(*doc, regressor_uuid, {});
    if(!proc)
      SKIP("score-addon-onnx is not built");
    auto& ctx = doc->context();
    const int base = proc->inlets().size();
    auto count = control(*proc, QStringLiteral("Param. count"));
    REQUIRE(count);
    CHECK(paramKnobs(*proc) == 0);

    for(int n : {3, 1, 5, 0, 4})
    {
      CAPTURE(n);
      CommandDispatcher<>{ctx.commandStack}.submit<Scenario::SetControllerControlValue>(
          *control(*proc, QStringLiteral("Param. count")), ossia::value{n}, ctx);
      settle();
      CHECK(paramKnobs(*proc) == n);
      CHECK(int(proc->inlets().size()) == base + n);
    }

    auto json = score::test::reload_via_json(app, *doc);
    REQUIRE(json);
    auto& processes = score::test::base_interval(*json).processes;
    auto it = ossia::find_if(processes, [&](auto& p) { return p.id() == proc->id(); });
    REQUIRE(it != processes.end());
    auto& reloaded = *it;
    CHECK(paramKnobs(reloaded) == 4);

    // A count changed on the reloaded process keeps working.
    auto& jctx = json->context();
    CommandDispatcher<>{jctx.commandStack}.submit<Scenario::SetControllerControlValue>(
        *control(reloaded, QStringLiteral("Param. count")), ossia::value{6}, jctx);
    settle();
    CHECK(paramKnobs(reloaded) == 6);
  });
}

TEST_CASE(
    "the rapidlib models' parameter count survives undo, redo and a binary reload",
    "[integration][dynamic][regressor]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    for(auto uuid : {regressor_uuid, QStringLiteral("039763f8-ea15-4900-9400-8c7f6a1c56cd")})
    {
      CAPTURE(uuid.toStdString());
      auto doc = score::test::new_document(app);
      auto proc = score::test::add_process(*doc, uuid, {});
      if(!proc)
        SKIP("score-addon-onnx is not built");
      auto& ctx = doc->context();
      const int base = proc->inlets().size();
      auto set = [&](score::Document& d, Process::ProcessModel& p, int n) {
        CommandDispatcher<>{d.context().commandStack}
            .submit<Scenario::SetControllerControlValue>(
                *control(p, QStringLiteral("Param. count")), ossia::value{n}, d.context());
        settle();
      };
      set(*doc, *proc, 2);
      set(*doc, *proc, 5);
      doc->commandStack().undo();
      settle();
      CHECK(int(proc->inlets().size()) == base + 2);
      doc->commandStack().redo();
      settle();
      CHECK(int(proc->inlets().size()) == base + 5);
      doc->commandStack().undo();
      doc->commandStack().undo();
      settle();
      CHECK(int(proc->inlets().size()) == base);
      set(*doc, *proc, 3);
      CHECK(int(proc->inlets().size()) == base + 3);

      auto bin = score::test::reload_via_bytes(app, *doc);
      REQUIRE(bin);
      auto& processes = score::test::base_interval(*bin).processes;
      auto it = ossia::find_if(processes, [&](auto& p) { return p.id() == proc->id(); });
      REQUIRE(it != processes.end());
      CHECK(int(it->inlets().size()) == base + 3);
      set(*bin, *it, 1);
      CHECK(int(it->inlets().size()) == base + 1);
      set(*bin, *it, 4);
      CHECK(int(it->inlets().size()) == base + 4);
      (void)ctx;
    }
  });
}

namespace
{
Process::ProcessModel* findProcess(score::Document& doc, const QString& uuid)
{
  const auto key = UuidKey<Process::ProcessModel>::fromString(uuid);
  for(auto& p : score::test::base_interval(doc).processes)
    if(p.concreteKey() == key)
      return &p;
  return nullptr;
}

void setCount(score::Document& doc, Process::ProcessModel& p, int n)
{
  auto& ctx = doc.context();
  CommandDispatcher<>{ctx.commandStack}.submit<Scenario::SetControllerControlValue>(
      *control(p, QStringLiteral("Param. count")), ossia::value{n}, ctx);
  settle();
}

float knobValue(Process::ProcessModel& p, int i)
{
  return valueOf(p, QStringLiteral("Param. %1").arg(i));
}
}

// regressor-docs.score is the reference example of the documentation, saved
// with one Regressor outlet where the spec has two: the port count differs from
// the spec on load, and every port is rebuilt from it. The rebuild must create
// the saved knobs too, or the next count change inserts past the end.
TEST_CASE(
    "a regressor saved before the spec gained a port keeps its knobs",
    "[integration][dynamic][regressor]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    QFile f{QStringLiteral(DYNAMIC_PORTS_DATA_DIR "/regressor-docs.score")};
    REQUIRE(f.open(QIODevice::ReadOnly));
    auto& delegates = app.interfaces<score::DocumentDelegateList>();
    auto doc = app.docManager.loadDocument(
        app, QStringLiteral("regressor-docs"), f.readAll(), JSONObject::type(),
        *delegates.begin());
    REQUIRE(doc);
    settle();
    auto proc = findProcess(*doc, regressor_uuid);
    if(!proc)
      SKIP("score-addon-onnx is not built");

    REQUIRE(paramKnobs(*proc) == 2);
    CHECK(control(*proc, QStringLiteral("Param. 0"))->id().val() == 17000);
    CHECK(knobValue(*proc, 0) == Catch::Approx(0.15694443881511688));
    CHECK(knobValue(*proc, 1) == Catch::Approx(0.7680555582046509));
    CHECK(proc->outlets().size() == 2);

    for(int n : {3, 1, 4})
    {
      CAPTURE(n);
      setCount(*doc, *proc, n);
      CHECK(paramKnobs(*proc) == n);
    }
    CHECK(knobValue(*proc, 0) == Catch::Approx(0.15694443881511688));
    doc->commandStack().undo();
    doc->commandStack().undo();
    doc->commandStack().undo();
    settle();
    REQUIRE(paramKnobs(*proc) == 2);
    CHECK(knobValue(*proc, 1) == Catch::Approx(0.7680555582046509));
  });
}

TEST_CASE(
    "a copied and pasted regressor keeps its knobs and their count still changes",
    "[integration][dynamic][regressor]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto proc = score::test::add_process(*doc, regressor_uuid, {});
    if(!proc)
      SKIP("score-addon-onnx is not built");
    auto& ctx = doc->context();
    setCount(*doc, *proc, 3);
    CommandDispatcher<>{ctx.commandStack}.submit<Process::SetValue>(
        *control(*proc, QStringLiteral("Param. 2")), ossia::value{0.25f});
    settle();

    ctx.selectionStack.pushNewSelection(Selection{proc});
    JSONReader r;
    REQUIRE(Scenario::copySelectedProcesses(r, ctx));
    const auto copied = r.toByteArray();

    auto check = [&](score::Document& target) {
      auto& tctx = target.context();
      auto& itv = score::test::base_interval(target);
      std::vector<Id<Process::ProcessModel>> before;
      for(auto& p : itv.processes)
        before.push_back(p.id());
      auto json = readJson(copied);
      CommandDispatcher<>{tctx.commandStack}.submit(
          new Scenario::Command::PasteProcessesInInterval{
              json, itv, ExpandMode::GrowShrink, QPointF{}});
      settle();
      Process::ProcessModel* pasted{};
      for(auto& p : itv.processes)
        if(!ossia::contains(before, p.id()))
          pasted = &p;
      REQUIRE(pasted);
      REQUIRE(paramKnobs(*pasted) == 3);
      CHECK(knobValue(*pasted, 2) == Catch::Approx(0.25f));
      setCount(target, *pasted, 5);
      CHECK(paramKnobs(*pasted) == 5);
      setCount(target, *pasted, 1);
      CHECK(paramKnobs(*pasted) == 1);
      target.commandStack().undo();
      target.commandStack().undo();
      settle();
      CHECK(paramKnobs(*pasted) == 3);
    };
    SECTION("in the same document") { check(*doc); }
    SECTION("in another document") { check(*score::test::new_document(app)); }
  });
}

TEST_CASE(
    "a regressor preset brings back its knob count and values",
    "[integration][dynamic][regressor]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto proc = score::test::add_process(*doc, regressor_uuid, {});
    if(!proc)
      SKIP("score-addon-onnx is not built");
    auto& ctx = doc->context();
    setCount(*doc, *proc, 3);
    CommandDispatcher<>{ctx.commandStack}.submit<Process::SetValue>(
        *control(*proc, QStringLiteral("Param. 2")), ossia::value{0.25f});
    settle();
    const auto preset = proc->savePreset();

    auto apply = [&] {
      auto& factories = app.interfaces<Process::LoadPresetCommandFactoryList>();
      auto cmd = factories.make(
          &Process::LoadPresetCommandFactory::make, *proc, preset, ctx);
      REQUIRE(cmd);
      CommandDispatcher<>{ctx.commandStack}.submit(cmd);
      settle();
    };

    SECTION("from fewer knobs")
    {
      setCount(*doc, *proc, 1);
      apply();
      REQUIRE(paramKnobs(*proc) == 3);
      CHECK(knobValue(*proc, 2) == Catch::Approx(0.25f));
      doc->commandStack().undo();
      settle();
      CHECK(paramKnobs(*proc) == 1);
      doc->commandStack().redo();
      settle();
      CHECK(paramKnobs(*proc) == 3);
    }
    SECTION("from more knobs")
    {
      setCount(*doc, *proc, 6);
      apply();
      CHECK(paramKnobs(*proc) == 3);
      doc->commandStack().undo();
      settle();
      CHECK(paramKnobs(*proc) == 6);
    }
    SECTION("on a new process")
    {
      auto other = score::test::add_process(*doc, regressor_uuid, {});
      REQUIRE(other);
      std::swap(proc, other);
      apply();
      REQUIRE(paramKnobs(*proc) == 3);
      CHECK(knobValue(*proc, 2) == Catch::Approx(0.25f));
    }
  });
}

TEST_CASE(
    "the regressor's knob count changes while it is playing",
    "[integration][dynamic][regressor]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto proc = score::test::add_process(*doc, regressor_uuid, {});
    if(!proc)
      SKIP("score-addon-onnx is not built");
    setCount(*doc, *proc, 2);

    auto& plug = startExecution(*doc);
    auto node = [&] {
      REQUIRE(plug.baseScenario());
      auto& procs = plug.baseScenario()->baseInterval().processes();
      auto it = procs.find(proc->id());
      REQUIRE(it != procs.end());
      REQUIRE(it->second->node);
      return it->second->node;
    };
    const int execBase = int(node()->root_inputs().size()) - 2;

    for(int n : {5, 0, 3, 1})
    {
      CAPTURE(n);
      setCount(*doc, *proc, n);
      runExec(plug);
      runExec(plug);
      CHECK(paramKnobs(*proc) == n);
      CHECK(int(node()->root_inputs().size()) == execBase + n);
    }
    doc->commandStack().undo();
    settle();
    runExec(plug);
    CHECK(paramKnobs(*proc) == 3);
    CHECK(int(node()->root_inputs().size()) == execBase + 3);

    // Saved and reloaded while playing
    auto json = score::test::reload_via_json(app, *doc);
    REQUIRE(json);
    auto reloaded = findProcess(*json, regressor_uuid);
    REQUIRE(reloaded);
    CHECK(paramKnobs(*reloaded) == 3);
    plug.clear();
    settle();
  });
}
