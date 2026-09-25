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

#include <QApplication>
#include <QElapsedTimer>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <catch2/catch_all.hpp>

#include <thread>

namespace
{
const QString composer_uuid = QStringLiteral("a4227e94-cf7d-4776-9aa0-2f384be7d97f");
const QString value_mixer_uuid = QStringLiteral("0fd108dd-daa8-4667-869d-f409da70e823");

void settle(int ms = 0)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QCoreApplication::sendPostedEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents(QEventLoop::AllEvents, 10);
  } while(t.elapsed() < ms);
  for(int i = 0; i < 5; i++)
  {
    QCoreApplication::sendPostedEvents();
    QApplication::processEvents();
  }
}

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

ossia::net::parameter_base* param(const score::DocumentContext& ctx, const State::Address& a)
{
  auto& dev = ctx.plugin<LocalTree::DocumentPlugin>().device();
  auto n = ossia::net::find_node(dev.get_root_node(), a.path.join('/').toStdString());
  return n ? n->get_parameter() : nullptr;
}

State::MessageList messages(const Scenario::StateModel& s)
{
  return Process::flatten(s.messages().rootNode());
}

Process::ControlInlet* control(Process::ProcessModel& p, const QString& name)
{
  for(auto in : p.inlets())
    if(in->name() == name)
      return qobject_cast<Process::ControlInlet*>(in);
  return nullptr;
}

float valueOf(Process::ProcessModel& p, const QString& name)
{
  auto c = control(p, name);
  REQUIRE(c);
  return ossia::convert<float>(c->value());
}

Scenario::StateModel& baseState(score::Document& doc, bool end)
{
  auto& base = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc).baseScenario();
  return end ? base.endState() : base.startState();
}

// As the execution does: addresses resolved first, messages sent from another thread
void play(const score::DocumentContext& ctx, const Scenario::StateModel& state)
{
  ossia::execution_state st;
  st.register_device(&ctx.plugin<LocalTree::DocumentPlugin>().device());
  st.apply_device_changes();
  auto s = Engine::score_to_ossia::state(state, st);
  std::thread{[&] { s.launch(); }}.join();
  settle(400);
}

// Writes into a published parameter from another thread, as a script does
void write(ossia::net::parameter_base& p, const ossia::value& v)
{
  std::thread{[&] { p.push_value(v); }}.join();
  settle();
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
    settle();
  }
  const score::DocumentContext& ctx() const { return doc->context(); }
  void setRows(const ossia::value& v)
  {
    CommandDispatcher<>{ctx().commandStack}.submit<Scenario::SetControllerControlValue>(
        *keywords(*proc), v, ctx());
    settle();
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

    auto kw = param(f.ctx(), LocalTree::scriptableAddress(*keywords(*f.proc)));
    REQUIRE(kw);
    write(*kw, rows({{12000, "a"}}));
    REQUIRE(knobs(*f.proc).size() == 1);
    settle(400);
    REQUIRE(stack.currentIndex() == before + 1);

    stack.undo();
    settle();
    REQUIRE(knobs(*f.proc).size() == 3);
    CHECK(valueOf(*f.proc, "b") == Catch::Approx(0.2f));
    CHECK(valueOf(*f.proc, "c") == Catch::Approx(0.3f));
    CHECK(LocalTree::scriptableAddress(*control(*f.proc, "c")) == f.address("c"));
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
    auto hidden = param(f.ctx(), f.address("c"));
    REQUIRE(hidden);
    write(*hidden, 0.9f);
    settle();

    stack.undo();
    settle();
    REQUIRE(knobs(*f.proc).size() == 3);
    CHECK(valueOf(*f.proc, "c") == Catch::Approx(0.3f));

    // A new edit that brings the knob back takes the written value
    stack.redo();
    settle();
    write(*param(f.ctx(), f.address("c")), 0.9f);
    settle();
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
    auto& cue = baseState(*f.doc, false);
    Scenario::Command::snapshotProcessInState(cue, *f.proc, f.ctx());
    settle();
    REQUIRE(messages(cue).size() == 4);

    f.setRows(rows({{12000, "a"}}));
    REQUIRE(knobs(*f.proc).size() == 1);
    CHECK(!f.ctx().plugin<LocalTree::DocumentPlugin>().references().broken().empty());

    play(f.ctx(), cue);
    REQUIRE(knobs(*f.proc).size() == 3);
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
    auto& cue = baseState(*f.doc, false);
    // As a document saved with a row "d" and opened without it
    CommandDispatcher<>{f.ctx().commandStack}.submit<Scenario::Command::AddMessagesToState>(
        cue, State::MessageList{
                 {State::AddressAccessor{f.address("d")}, 0.7f},
                 {State::AddressAccessor{LocalTree::scriptableAddress(*keywords(*f.proc))},
                  rows({{12000, "a"}, {12003, "d"}})}});
    settle();

    play(f.ctx(), cue);
    REQUIRE(knobs(*f.proc).size() == 2);
    CHECK(valueOf(*f.proc, "d") == Catch::Approx(0.7f));
    CHECK(LocalTree::scriptableAddress(*control(*f.proc, "d")) == f.address("d"));
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
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    plug.runAllCommands();
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

    auto kw = param(f.ctx(), LocalTree::scriptableAddress(*keywords(*f.proc)));
    REQUIRE(kw);
    write(*kw, rows({{12000, "a"}, {12001, "b"}, {12002, "c"}, {12003, "d"}}));
    REQUIRE(knobs(*f.proc).size() == 4);
    settle(400);
    REQUIRE(stack.currentIndex() == before + 1);

    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    plug.runAllCommands();
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    plug.clear();
    settle();
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
    settle();
    disp.submit<Process::SetValue>(*control(*mixer, "Mix 2"), ossia::value{0.25f});
    settle();

    auto& cue = baseState(*doc, false);
    Scenario::Command::snapshotProcessInState(cue, *mixer, ctx);
    settle();

    disp.submit<Scenario::SetControllerControlValue>(*count, ossia::value{1}, ctx);
    settle();
    REQUIRE(!control(*mixer, "Mix 2"));

    play(ctx, cue);
    REQUIRE(control(*mixer, "Mix 2"));
    CHECK(valueOf(*mixer, "Mix 2") == Catch::Approx(0.25f));
  });
}

namespace
{
void runExec(Execution::DocumentPlugin& plug)
{
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  plug.runAllCommands();
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
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
    auto& cue = baseState(*f.doc, false);
    Scenario::Command::snapshotProcessInState(cue, *f.proc, f.ctx());
    settle();
    f.setRows(rows({{12000, "a"}}));

    auto& plug = startExecution(*f.doc);
    {
      auto s = Engine::score_to_ossia::state(cue, *plug.context().execState);
      std::thread{[&] { s.launch(); }}.join();
    }
    settle(400);
    runExec(plug);
    runExec(plug);
    REQUIRE(knobs(*f.proc).size() == 3);

    auto& object = executed(plug, *f.proc);
    object();
    CHECK(object.outputs.out.value == "(a:0.1), (b:0.2), (c:0.3)");
    plug.clear();
    settle();
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
    settle();
    runExec(plug);
    f.doc->commandStack().undo();
    settle();
    runExec(plug);
    f.proc = &itv.processes.at(id);
    f.setRows(rows({{12000, "a"}, {12001, "b"}, {12002, "c"}}));

    auto b = control(*f.proc, "b");
    REQUIRE(b);
    CHECK(b->exposed() == QStringLiteral("b"));
    CHECK(LocalTree::scriptableAddress(*b) == f.address("b"));
    plug.clear();
    settle();
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
    auto kw = param(f.ctx(), LocalTree::scriptableAddress(*keywords(*f.proc)));
    REQUIRE(kw);

    std::thread{[&] {
      kw->push_value(rows({{12000, "a"}, {12001, "b"}}));
      kw->push_value(rows({{12000, "a"}}));
      kw->push_value(rows({{12000, "a"}, {12003, "d"}}));
    }}.join();
    settle();
    // Applied at once, committed as one edit once the writes stop
    REQUIRE(knobs(*f.proc).size() == 2);
    CHECK(!stack.isAtSavedIndex());
    settle(400);
    REQUIRE(stack.currentIndex() == before + 1);

    stack.undo();
    settle();
    REQUIRE(knobs(*f.proc).size() == 3);
    CHECK(valueOf(*f.proc, "c") == Catch::Approx(0.3f));

    // A float, e.g. from an automation, into the list of rows
    write(*kw, ossia::value{0.5f});
    settle(400);
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
    auto& scenar = *[&] {
      for(auto& p : score::test::base_interval(*f.doc).processes)
        if(auto sc = qobject_cast<Scenario::ProcessModel*>(&p))
          return sc;
      FAIL("no scenario");
      return (Scenario::ProcessModel*)nullptr;
    }();
    auto create = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        scenar, scenar.states.begin()->id(), TimeVal::fromMsecs(2000), 0.3, false};
    const auto itvId = create->createdInterval();
    CommandDispatcher<>{f.ctx().commandStack}.submit(create);
    auto& itv = scenar.interval(itvId);
    auto& start = scenar.state(itv.startState());
    auto& end = scenar.state(itv.endState());

    Scenario::Command::snapshotProcessInState(start, *f.proc, f.ctx());
    settle();
    f.setRows(rows({{12000, "a"}, {12001, "b"}}));
    CommandDispatcher<>{f.ctx().commandStack}.submit<Process::SetValue>(
        *control(*f.proc, "a"), ossia::value{0.9f});
    Scenario::Command::snapshotProcessInState(end, *f.proc, f.ctx());
    settle();

    Scenario::Command::InterpolateStates({&itv}, f.ctx().commandStack);
    settle();
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
    auto& cue = baseState(*f.doc, false);
    CommandDispatcher<>{f.ctx().commandStack}.submit<Scenario::Command::AddMessagesToState>(
        cue, State::MessageList{{*State::parseAddressAccessor("score:/controls/pc/*"), 0.7f}});
    settle();
    play(f.ctx(), cue);
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
    settle();
    REQUIRE(knobs(*f.proc).size() == 1);
    f.doc->commandStack().undo();
    settle();
    REQUIRE(knobs(*f.proc).size() == 3);
    CHECK(valueOf(*f.proc, "b") == Catch::Approx(0.2f));
  });
}
