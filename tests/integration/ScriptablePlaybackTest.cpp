// Values written into a published control, stopped, playing and recorded

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/ProcessState.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <ossia/dataflow/nodes/state.hpp>
#include <JS/Commands/EditScript.hpp>
#include <JS/JSProcessModel.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <LocalTree/ScriptableProcessComponent.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Nodal/Commands.hpp>
#include <Nodal/Process.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Commands/Event/SetCondition.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Commands/TimeSync/SetAutoTrigger.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <LocalTree/ScriptableScenarioComponent.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/CommandData.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/ComponentUtils.hpp>
#include <score/plugins/UuidKey.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/editor/scenario/time_sync.hpp>
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
const QString smooth_uuid = QStringLiteral("bf603921-5a48-4aa5-9bc1-48a762be6467");
const QString js_uuid = QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0");

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
    QApplication::processEvents(QEventLoop::AllEvents, 10);
  }
}

// Waits longer than the delay that closes a burst of edits
void settleBurst()
{
  settle(400);
}

Process::ControlInlet& control(Process::ProcessModel& p, const QString& name)
{
  for(auto inlet : p.inlets())
    if(auto ctl = qobject_cast<Process::ControlInlet*>(inlet); ctl && ctl->name() == name)
      return *ctl;
  FAIL("no control named " << name.toStdString());
  throw;
}

ossia::net::parameter_base& param(const score::DocumentContext& ctx, const State::Address& a)
{
  auto& dev = ctx.plugin<LocalTree::DocumentPlugin>().device();
  auto n = ossia::net::find_node(dev.get_root_node(), a.path.join('/').toStdString());
  REQUIRE(n);
  REQUIRE(n->get_parameter());
  return *n->get_parameter();
}

// Writes from another thread, as the execution or a remote client does
void write(ossia::net::parameter_base& p, const ossia::value& v)
{
  std::thread{[&] { p.push_value(v); }}.join();
  settle();
}

struct Playback
{
  Execution::DocumentPlugin& plug;
  score::Document& doc;
  explicit Playback(score::Document& d)
      : plug{d.context().plugin<Execution::DocumentPlugin>()}
      , doc{d}
  {
    plug.reload(true, score::test::base_interval(d));
    run();
  }
  void run()
  {
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    plug.runAllCommands();
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  }
  void stop()
  {
    plug.clear();
    settle();
  }
};

Scenario::EditionSettings& editionSettings(const score::GUIApplicationContext& ctx)
{
  return ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>().editionSettings();
}

struct Fixture
{
  score::Document* doc{};
  Process::ProcessModel* proc{};
  Process::ControlInlet* amount{};
  Process::ControlInlet* beta{};
  explicit Fixture(const score::GUIApplicationContext& ctx)
  {
    doc = score::test::new_document(ctx);
    proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    amount = &control(*proc, QStringLiteral("Amount"));
    beta = &control(*proc, QStringLiteral("Beta (1e only)"));
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::SetProcessScriptable>(*proc, true);
    disp.submit<Process::SetValue>(*amount, ossia::value{0.2f});
    disp.submit<Process::SetValue>(*beta, ossia::value{1.f});
    settle();
  }
  const score::DocumentContext& ctx() const { return doc->context(); }
  ossia::net::parameter_base& amountParam() const
  {
    return param(ctx(), LocalTree::scriptableAddress(*amount));
  }
  ossia::net::parameter_base& betaParam() const
  {
    return param(ctx(), LocalTree::scriptableAddress(*beta));
  }
  int commands() const { return doc->commandStack().currentIndex(); }
};
}

TEST_CASE(
    "while playing, a write into a published control only drives the execution",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(false);
    Fixture f{ctx};
    std::optional<ossia::value> shown;
    auto showing = QObject::connect(
        f.amount, &Process::ControlInlet::executionValueChanged, f.amount,
        [&](const ossia::value& v) { shown = v; });
    const int before = f.commands();

    Playback play{*f.doc};
    write(f.amountParam(), ossia::value{0.9f});
    settleBurst();
    REQUIRE(f.amount->value() == ossia::value{0.2f});
    REQUIRE(shown == ossia::value{0.9f});
    REQUIRE(f.amountParam().value() == ossia::value{0.9f});
    REQUIRE(f.commands() == before);

    // Editing the control while playing still reaches the execution
    CommandDispatcher<>{f.ctx().commandStack}.submit<Process::SetValue>(
        *f.amount, ossia::value{0.4f});
    REQUIRE(f.amountParam().value() == ossia::value{0.4f});

    // After stop, the parameter holds the control's value
    write(f.amountParam(), ossia::value{0.8f});
    play.stop();
    settleBurst();
    REQUIRE(f.amount->value() == ossia::value{0.4f});
    REQUIRE(f.amountParam().value() == ossia::value{0.4f});
    REQUIRE(f.commands() == before + 1);
    QObject::disconnect(showing);
  });
}

TEST_CASE(
    "with Record playback on, the controls keep what playback wrote, as one edit",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(true);
    Fixture f{ctx};
    const int before = f.commands();

    Playback play{*f.doc};
    write(f.amountParam(), ossia::value{0.9f});
    write(f.amountParam(), ossia::value{0.7f});
    write(f.betaParam(), ossia::value{5.f});
    REQUIRE(f.amount->value() == ossia::value{0.7f});
    REQUIRE(f.beta->value() == ossia::value{5.f});
    REQUIRE(f.commands() == before);

    play.stop();
    REQUIRE(f.commands() == before + 1);
    REQUIRE(f.amount->value() == ossia::value{0.7f});

    f.doc->commandStack().undo();
    REQUIRE(f.amount->value() == ossia::value{0.2f});
    REQUIRE(f.beta->value() == ossia::value{1.f});
    REQUIRE(f.amountParam().value() == ossia::value{0.2f});
    f.doc->commandStack().redo();
    REQUIRE(f.amount->value() == ossia::value{0.7f});
    REQUIRE(f.beta->value() == ossia::value{5.f});

    Playback again{*f.doc};
    again.stop();
    REQUIRE(f.commands() == before + 1);
    editionSettings(ctx).setRecordPlayback(false);
  });
}

TEST_CASE(
    "without Record playback, what playback wrote can be kept afterwards, as one edit",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(false);
    Fixture f{ctx};
    auto& tree = f.ctx().plugin<LocalTree::DocumentPlugin>();
    int changes = 0;
    auto counting = QObject::connect(
        &tree, &LocalTree::ScriptableTreeBase::playedValuesChanged, &tree,
        [&] { changes++; });
    const int before = f.commands();
    REQUIRE(!tree.hasPlayedValues());

    Playback play{*f.doc};
    write(f.amountParam(), ossia::value{0.9f});
    write(f.amountParam(), ossia::value{0.7f});
    write(f.betaParam(), ossia::value{5.f});
    REQUIRE(tree.hasPlayedValues());
    REQUIRE(changes == 1);
    play.stop();
    settleBurst();
    REQUIRE(f.amount->value() == ossia::value{0.2f});
    REQUIRE(tree.hasPlayedValues());
    REQUIRE(f.commands() == before);

    tree.keepPlayedValues();
    REQUIRE(!tree.hasPlayedValues());
    REQUIRE(changes == 2);
    REQUIRE(f.commands() == before + 1);
    REQUIRE(f.amount->value() == ossia::value{0.7f});
    REQUIRE(f.beta->value() == ossia::value{5.f});
    REQUIRE(f.amountParam().value() == ossia::value{0.7f});

    f.doc->commandStack().undo();
    REQUIRE(f.amount->value() == ossia::value{0.2f});
    REQUIRE(f.beta->value() == ossia::value{1.f});
    f.doc->commandStack().redo();
    REQUIRE(f.amount->value() == ossia::value{0.7f});
    REQUIRE(f.beta->value() == ossia::value{5.f});

    tree.keepPlayedValues();
    REQUIRE(f.commands() == before + 1);

    // A new run clears the values played by the previous one
    Playback second{*f.doc};
    write(f.amountParam(), ossia::value{0.3f});
    second.stop();
    REQUIRE(tree.hasPlayedValues());
    Playback third{*f.doc};
    REQUIRE(!tree.hasPlayedValues());
    third.stop();

    // Keep while still playing
    Playback fourth{*f.doc};
    write(f.amountParam(), ossia::value{0.5f});
    tree.keepPlayedValues();
    REQUIRE(f.amount->value() == ossia::value{0.5f});
    REQUIRE(f.amountParam().value() == ossia::value{0.5f});
    fourth.stop();
    settleBurst();
    REQUIRE(f.amount->value() == ossia::value{0.5f});
    REQUIRE(f.commands() == before + 2);
    QObject::disconnect(counting);
  });
}

TEST_CASE(
    "turning Record playback on while playing records what follows only",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(false);
    Fixture f{ctx};
    const int before = f.commands();

    Playback play{*f.doc};
    write(f.amountParam(), ossia::value{0.9f});
    write(f.betaParam(), ossia::value{5.f});
    REQUIRE(f.amount->value() == ossia::value{0.2f});

    editionSettings(ctx).setRecordPlayback(true);
    write(f.amountParam(), ossia::value{0.6f});
    REQUIRE(f.amount->value() == ossia::value{0.6f});
    play.stop();
    editionSettings(ctx).setRecordPlayback(false);

    REQUIRE(f.commands() == before + 1);
    REQUIRE(f.amount->value() == ossia::value{0.6f});
    REQUIRE(f.beta->value() == ossia::value{1.f});
    REQUIRE(f.betaParam().value() == ossia::value{1.f});
    f.doc->commandStack().undo();
    REQUIRE(f.amount->value() == ossia::value{0.2f});
  });
}

TEST_CASE(
    "while stopped, a burst of writes into a published control is one edit",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(false);
    Fixture f{ctx};
    const int before = f.commands();

    for(float v : {0.3f, 0.4f, 0.5f})
      write(f.amountParam(), ossia::value{v});
    REQUIRE(f.amount->value() == ossia::value{0.5f});
    settleBurst();
    REQUIRE(f.commands() == before + 1);

    write(f.amountParam(), ossia::value{0.6f});
    settleBurst();
    REQUIRE(f.commands() == before + 2);

    f.doc->commandStack().undo();
    REQUIRE(f.amount->value() == ossia::value{0.5f});
    REQUIRE(f.amountParam().value() == ossia::value{0.5f});
    f.doc->commandStack().undo();
    REQUIRE(f.amount->value() == ossia::value{0.2f});

    write(f.amountParam(), ossia::value{0.2f});
    settleBurst();
    REQUIRE(f.commands() == before);
  });
}

TEST_CASE(
    "while playing, the state of a process still applies to the process",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(false);
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    auto js = qobject_cast<JS::ProcessModel*>(score::test::add_process(*doc, js_uuid, {}));
    REQUIRE(js);
    CommandDispatcher<> disp{dctx.commandStack};
    disp.submit(new JS::EditScript{
        *js, JS::QmlSource{QStringLiteral(R"_(import Score
Script {
  FloatSlider { objectName: "level"; min: 0; max: 10; init: 1 }
  tick: function(token, state) { }
})_"),
                           {}},
        dctx});
    disp.submit<Process::SetProcessScriptable>(*js, true);
    const JS::JSState steps{{QStringLiteral("step"), ossia::value{3}}};
    js->setState(steps);
    const auto saved = Process::stateBeyondControls(*js);
    js->setState({});
    settle();

    Playback play{*doc};
    write(param(dctx, LocalTree::scriptableStateAddress(*js)), saved.toStdString());
    REQUIRE(js->state() == steps);
    play.stop();
  });
}

TEST_CASE(
    "the Record playback mode is one setting for every document",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto& settings = editionSettings(ctx);
    settings.setRecordPlayback(false);
    int changes = 0;
    auto c = QObject::connect(
        &settings, &Scenario::EditionSettings::recordPlaybackChanged, &settings,
        [&](bool) { changes++; });
    settings.setRecordPlayback(true);
    settings.setRecordPlayback(true);
    REQUIRE(settings.recordPlayback());
    REQUIRE(changes == 1);

    Fixture f{ctx};
    Playback play{*f.doc};
    write(f.amountParam(), ossia::value{0.9f});
    REQUIRE(f.amount->value() == ossia::value{0.9f});
    play.stop();
    settings.setRecordPlayback(false);
    QObject::disconnect(c);
  });
}

namespace
{
Scenario::StateModel& baseState(score::Document& doc, bool end)
{
  auto& base = doc.context().model<Scenario::ScenarioDocumentModel>().baseScenario();
  return end ? base.endState() : base.startState();
}

void putInState(Fixture& f, Scenario::StateModel& state)
{
  CommandDispatcher<>{f.ctx().commandStack}.submit<Scenario::Command::AddMessagesToState>(
      state,
      State::MessageList{
          {State::AddressAccessor{LocalTree::scriptableAddress(*f.amount)}, 0.9f},
          {State::AddressAccessor{LocalTree::scriptableAddress(*f.beta)}, 5.f}});
}
}

TEST_CASE(
    "the end state sent at stop leaves the controls alone, unless recording",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(false);
    Fixture f{ctx};
    putInState(f, baseState(*f.doc, true));
    auto& tree = f.ctx().plugin<LocalTree::DocumentPlugin>();
    const int before = f.commands();

    // Recall the stop state, as stopping does
    auto stop = [&](Playback& play) {
      tree.beginRecall(LocalTree::Recall::Stop);
      play.plug.playStopState();
      play.stop();
      tree.endRecall(LocalTree::Recall::Stop);
      settleBurst();
    };

    {
      Playback play{*f.doc};
      stop(play);
    }
    REQUIRE(f.amount->value() == ossia::value{0.2f});
    REQUIRE(f.beta->value() == ossia::value{1.f});
    REQUIRE(f.amountParam().value() == ossia::value{0.2f});
    REQUIRE(f.commands() == before);

    editionSettings(ctx).setRecordPlayback(true);
    {
      Playback play{*f.doc};
      stop(play);
    }
    editionSettings(ctx).setRecordPlayback(false);
    REQUIRE(f.amount->value() == ossia::value{0.9f});
    REQUIRE(f.beta->value() == ossia::value{5.f});
    REQUIRE(f.commands() == before + 1);
    f.doc->commandStack().undo();
    REQUIRE(f.amount->value() == ossia::value{0.2f});
    REQUIRE(f.beta->value() == ossia::value{1.f});
  });
}

TEST_CASE(
    "a state played while stopped sets the controls as one edit",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(false);
    Fixture f{ctx};
    putInState(f, baseState(*f.doc, false));
    auto& exec = f.ctx().plugin<Execution::DocumentPlugin>();
    const int before = f.commands();

    // Recall the start state, as "Play (States)" and reinitializing do
    {
      LocalTree::StateRecall recall{f.ctx(), exec};
      exec.playStartState();
    }
    settleBurst();
    REQUIRE(f.amount->value() == ossia::value{0.9f});
    REQUIRE(f.beta->value() == ossia::value{5.f});
    REQUIRE(f.commands() == before + 1);

    f.doc->commandStack().undo();
    REQUIRE(f.amount->value() == ossia::value{0.2f});
    REQUIRE(f.beta->value() == ossia::value{1.f});
    f.doc->commandStack().redo();
    REQUIRE(f.amount->value() == ossia::value{0.9f});

    // While playing, recalling a state adds no command
    Playback play{*f.doc};
    CommandDispatcher<>{f.ctx().commandStack}.submit<Process::SetValue>(
        *f.amount, ossia::value{0.2f});
    const int playing = f.commands();
    {
      LocalTree::StateRecall recall{f.ctx(), exec};
      exec.playStartState();
    }
    settleBurst();
    REQUIRE(f.amount->value() == ossia::value{0.2f});
    REQUIRE(f.commands() == playing);
    play.stop();
  });
}

TEST_CASE(
    "a flood of values from another thread reaches the control as its latest",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(false);
    Fixture f{ctx};
    const int before = f.commands();
    auto& p = f.amountParam();
    std::thread{[&] {
      for(int i = 1; i <= 2000; i++)
        p.push_value(float(i) / 2000.f);
    }}.join();
    settleBurst();
    REQUIRE(f.amount->value() == ossia::value{1.f});
    REQUIRE(f.commands() == before + 1);
    f.doc->commandStack().undo();
    REQUIRE(f.amount->value() == ossia::value{0.2f});
  });
}

namespace
{
ossia::net::parameter_base* boundParameter(Playback& play, Process::Inlet& port)
{
  auto& setup = play.plug.context().setup;
  auto it = setup.inlets.find(&port);
  if(it == setup.inlets.end())
    return nullptr;
  auto p = it->second.second->address.target<ossia::net::parameter_base*>();
  return p ? *p : nullptr;
}

bool anyRetired(const score::DocumentContext& ctx)
{
  bool found = false;
  auto visit = [&](auto& self, const ossia::net::node_base& n) -> void {
    if(n.get_name().find("(retired)") != std::string::npos)
      found = true;
    for(auto& child : n.children())
      self(self, *child);
  };
  visit(visit, ctx.plugin<LocalTree::DocumentPlugin>().device().get_root_node());
  return found;
}
}

TEST_CASE(
    "a process removed and brought back by undo while playing keeps its parameters",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(false);
    Fixture f{ctx};
    auto& itv = score::test::base_interval(*f.doc);
    const auto procId = f.proc->id();
    const auto addr = LocalTree::scriptableAddress(*f.amount);
    auto* original = &f.amountParam();
    const auto stateAddr = LocalTree::scriptableStateAddress(*f.proc);

    Playback play{*f.doc};
    REQUIRE(boundParameter(play, *f.amount) == original);

    CommandDispatcher<>{f.ctx().commandStack}.submit(
        new Scenario::Command::RemoveProcessFromInterval{itv, procId});
    play.run();
    settle();
    REQUIRE(!LocalTree::published(addr, f.ctx()).value_or(false));

    f.doc->commandStack().undo();
    play.run();
    settle();
    play.run();

    // Same parameter object as before, which the running execution still uses
    auto& proc = itv.processes.at(procId);
    auto& amount = control(proc, QStringLiteral("Amount"));
    REQUIRE(LocalTree::scriptableAddress(amount) == addr);
    REQUIRE(&param(f.ctx(), addr) == original);
    REQUIRE(LocalTree::scriptableStateAddress(proc) == stateAddr);
    REQUIRE(boundParameter(play, amount) == original);

    // and values written to it reach the process
    std::optional<ossia::value> shown;
    auto showing = QObject::connect(
        &amount, &Process::ControlInlet::executionValueChanged, &amount,
        [&](const ossia::value& v) { shown = v; });
    write(*original, ossia::value{0.9f});
    REQUIRE(shown == ossia::value{0.9f});
    QObject::disconnect(showing);

    f.doc->commandStack().redo();
    play.run();
    settle();
    f.doc->commandStack().undo();
    play.run();
    settle();
    play.run();
    REQUIRE(&param(f.ctx(), addr) == original);
    REQUIRE(boundParameter(play, control(itv.processes.at(procId), QStringLiteral("Amount")))
            == original);

    play.stop();
    REQUIRE(!anyRetired(f.ctx()));
  });
}

TEST_CASE(
    "a nodal child removed while playing leaves the graph, and comes back by undo",
    "[integration][scriptable][playback][nodal]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(false);
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    auto nodal = qobject_cast<Nodal::Model*>(score::test::add_process(
        *doc, QStringLiteral("f5678806-c431-45c5-ae3a-fae5183380fb"), {}));
    REQUIRE(nodal);
    CommandDispatcher<> disp{dctx.commandStack};
    auto create = new Nodal::CreateNode{
        *nodal, QPointF{}, UuidKey<Process::ProcessModel>{score::uuids::string_generator::compute("bf603921-5a48-4aa5-9bc1-48a762be6467")},
        {}, Id<Process::ProcessModel>{1}};
    const auto childId = create->nodeId();
    disp.submit(create);
    auto& child = nodal->nodes.at(childId);
    disp.submit<Process::RenameProcess>(child, QStringLiteral("inner"));
    disp.submit<Process::SetProcessScriptable>(child, true);
    settle();
    const auto addr = LocalTree::scriptableAddress(control(child, QStringLiteral("Amount")));
    REQUIRE(addr.isSet());
    auto* original = &param(dctx, addr);

    Playback play{*doc};
    settle();
    play.run();
    auto& amount = control(child, QStringLiteral("Amount"));
    REQUIRE(boundParameter(play, amount) == original);
    auto graphNode = play.plug.context().setup.inlets.at(&amount).first;
    auto inGraph = [&](const ossia::graph_node* n) {
      return ossia::contains(play.plug.context().execGraph->get_nodes(), n);
    };
    REQUIRE(inGraph(graphNode.get()));

    disp.submit(new Nodal::RemoveNode{*nodal, child});
    play.run();
    settle();
    play.run();
    REQUIRE(!inGraph(graphNode.get()));

    doc->commandStack().undo();
    play.run();
    settle();
    play.run();
    auto& back = control(nodal->nodes.at(childId), QStringLiteral("Amount"));
    REQUIRE(&param(dctx, addr) == original);
    REQUIRE(boundParameter(play, back) == original);
    REQUIRE(inGraph(play.plug.context().setup.inlets.at(&back).first.get()));

    play.stop();
    REQUIRE(!anyRetired(dctx));
  });
}

TEST_CASE(
    "while playing, a trigger fired through the namespace reaches the execution directly",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    Scenario::ProcessModel* scenar{};
    for(auto& p : score::test::base_interval(*doc).processes)
      if(auto sc = qobject_cast<Scenario::ProcessModel*>(&p))
        scenar = sc;
    REQUIRE(scenar);
    auto& sync = *scenar->timeSyncs.begin();
    CommandDispatcher<>{dctx.commandStack}.submit<Scenario::Command::SetTimeSyncScriptable>(
        sync, true);
    settle();
    auto& trigger = param(dctx, LocalTree::scriptableAddress(sync));
    auto published = score::findComponent<LocalTree::ScriptableTimeSync>(sync.components());
    REQUIRE(published);
    int viaInterface = 0;
    auto counting = QObject::connect(
        &sync, &Scenario::TimeSyncModel::triggeredByGui, &sync, [&] { viaInterface++; });

    {
      Playback play{*doc};
      settle();
      REQUIRE(published->execution()->attached());

      // From another thread, the trigger bypasses the user interface
      std::thread{[&] { trigger.push_value(ossia::impulse{}); }}.join();
      settle();
      REQUIRE(viaInterface == 0);
      play.stop();
    }

    // When stopped, it goes through the user interface
    REQUIRE(!published->execution()->attached());
    std::thread{[&] { trigger.push_value(ossia::impulse{}); }}.join();
    settle();
    REQUIRE(viaInterface == 1);
    QObject::disconnect(counting);
  });
}

TEST_CASE(
    "while playing, a cable into a published control wins over what is written to it",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(false);
    Fixture f{ctx};
    auto autom = score::test::add_process(
        *f.doc, QStringLiteral("d2a67bd8-5d3f-404e-b6e9-e350cf2a833f"), {});
    REQUIRE(autom);
    auto& dp = f.ctx().model<Scenario::ScenarioDocumentModel>();
    CommandDispatcher<>{f.ctx().commandStack}.submit(new Dataflow::CreateCable{
        dp, Id<Process::Cable>{4242}, Process::CableType::ImmediateGlutton,
        *autom->outlets()[0], *f.amount});
    settle();
    REQUIRE(!f.amount->cables().empty());

    std::optional<ossia::value> shown;
    auto showing = QObject::connect(
        f.amount, &Process::ControlInlet::executionValueChanged, f.amount,
        [&](const ossia::value& v) { shown = v; });
    const int before = f.commands();

    Playback play{*f.doc};
    write(f.amountParam(), ossia::value{0.9f});
    REQUIRE(!shown);
    REQUIRE(f.amount->value() == ossia::value{0.2f});
    REQUIRE(f.commands() == before);

    // Without the cable, the control plays the written value
    f.doc->commandStack().undo();
    settle();
    REQUIRE(f.amount->cables().empty());
    write(f.amountParam(), ossia::value{0.7f});
    REQUIRE(shown == ossia::value{0.7f});
    play.stop();
    QObject::disconnect(showing);
  });
}


TEST_CASE(
    "a write made while playing and received after stop is not an edit",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(false);
    Fixture f{ctx};
    const int before = f.commands();

    Playback play{*f.doc};
    // Received by the GUI thread only after the stop
    std::thread{[&] { f.amountParam().push_value(ossia::value{0.9f}); }}.join();
    play.stop();
    settleBurst();
    REQUIRE(f.amount->value() == ossia::value{0.2f});
    REQUIRE(f.amountParam().value() == ossia::value{0.2f});
    REQUIRE(f.commands() == before);
  });
}

TEST_CASE(
    "an edit written while stopped is kept when the control is unpublished right after",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    const int before = f.commands();

    write(f.amountParam(), ossia::value{0.6f});
    CommandDispatcher<>{f.ctx().commandStack}.submit<Process::SetProcessScriptable>(
        *f.proc, false);
    settleBurst();
    REQUIRE(f.amount->value() == ossia::value{0.6f});
    REQUIRE(f.commands() == before + 2);

    f.doc->commandStack().undo();
    REQUIRE(f.amount->value() == ossia::value{0.2f});
  });
}

TEST_CASE(
    "a document is modified from the first write of a burst, before its edit is committed",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& stack = f.doc->commandStack();
    stack.markCurrentIndexAsSaved();
    REQUIRE(stack.isAtSavedIndex());
    const int before = f.commands();

    write(f.amountParam(), ossia::value{0.6f});
    REQUIRE(f.commands() == before);
    REQUIRE(!stack.isAtSavedIndex());

    settleBurst();
    REQUIRE(f.commands() == before + 1);
    REQUIRE(!stack.isAtSavedIndex());

    stack.undo();
    REQUIRE(stack.isAtSavedIndex());

    // A burst that ends where it started leaves the document as it was
    write(f.amountParam(), ossia::value{0.5f});
    write(f.amountParam(), ossia::value{0.2f});
    settleBurst();
    REQUIRE(f.commands() == before);
    REQUIRE(stack.isAtSavedIndex());
  });
}

TEST_CASE(
    "saving while writes are pending leaves the document saved until they are committed",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& stack = f.doc->commandStack();
    write(f.amountParam(), ossia::value{0.6f});
    REQUIRE(!stack.isAtSavedIndex());
    // As a save, or an exit forced without saving
    stack.markCurrentIndexAsSaved();
    CHECK(stack.isAtSavedIndex());

    settleBurst();
    // The edit is committed after the save
    CHECK(!stack.isAtSavedIndex());
    stack.undo();
    CHECK(stack.isAtSavedIndex());
  });
}

TEST_CASE(
    "undoing during a burst of writes drops the burst rather than overriding the undo",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& stack = f.doc->commandStack();
    write(f.amountParam(), ossia::value{0.6f});
    settleBurst();
    const int after = stack.currentIndex();

    write(f.amountParam(), ossia::value{0.7f});
    stack.undo();
    settleBurst();
    CHECK(stack.currentIndex() == after - 1);
    CHECK(stack.canRedo());
    CHECK(f.amount->value() == ossia::value{0.2f});
  });
}

TEST_CASE(
    "a state gets the messages of a control published while playing",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& end = score::IDocument::get<Scenario::ScenarioDocumentModel>(*f.doc)
                    .baseScenario()
                    .endState();
    CommandDispatcher<>{f.ctx().commandStack}.submit<Scenario::Command::AddMessagesToState>(
        end, State::MessageList{
                 {State::AddressAccessor{LocalTree::scriptableAddress(*f.amount)}, 0.9f}});
    CommandDispatcher<>{f.ctx().commandStack}.submit<Process::SetProcessScriptable>(
        *f.proc, false);
    settle();

    Playback play{*f.doc};
    auto& node = *play.plug.baseScenario()->endState().node();
    CHECK(node.data.size() == 0);

    CommandDispatcher<>{f.ctx().commandStack}.submit<Process::SetProcessScriptable>(
        *f.proc, true);
    settle();
    play.run();
    CHECK(node.data.size() == 1);
    play.stop();
  });
}

TEST_CASE(
    "while playing, a control with an address of its own ignores writes to its published address",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    editionSettings(ctx).setRecordPlayback(false);
    Fixture f{ctx};
    CommandDispatcher<>{f.ctx().commandStack}.submit<Process::ChangePortAddress>(
        *f.amount, *State::parseAddressAccessor("score:/controls/elsewhere"));
    settle();
    std::optional<ossia::value> shown;
    auto showing = QObject::connect(
        f.amount, &Process::ControlInlet::executionValueChanged, f.amount,
        [&](const ossia::value& v) { shown = v; });

    Playback play{*f.doc};
    write(f.amountParam(), ossia::value{0.9f});
    CHECK(!shown);
    play.stop();
    QObject::disconnect(showing);
  });
}

TEST_CASE(
    "the edits made by playback replay from their saved form",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto replayLast = [](score::Document& doc) {
      auto& stack = doc.commandStack();
      REQUIRE(stack.canUndo());
      const score::CommandData data{*stack.command(stack.currentIndex() - 1)};
      stack.undo();
      settle();
      std::unique_ptr<score::Command> copy{
          doc.context().app.components.instantiateUndoCommand(data)};
      REQUIRE(copy);
      copy->redo(doc.context());
      settle();
      stack.push(copy.release());
    };

    editionSettings(ctx).setRecordPlayback(true);
    Fixture f{ctx};
    {
      Playback play{*f.doc};
      write(f.amountParam(), ossia::value{0.7f});
      play.stop();
    }
    editionSettings(ctx).setRecordPlayback(false);
    REQUIRE(f.amount->value() == ossia::value{0.7f});
    replayLast(*f.doc);
    CHECK(f.amount->value() == ossia::value{0.7f});

    {
      Playback play{*f.doc};
      write(f.amountParam(), ossia::value{0.3f});
      play.stop();
    }
    auto& tree = f.ctx().plugin<LocalTree::DocumentPlugin>();
    REQUIRE(tree.hasPlayedValues());
    tree.keepPlayedValues();
    settle();
    REQUIRE(f.amount->value() == ossia::value{0.3f});
    replayLast(*f.doc);
    CHECK(f.amount->value() == ossia::value{0.3f});
  });
}

TEST_CASE(
    "each run starts with the published conditions false",
    "[integration][scriptable][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    Scenario::ProcessModel* scenar{};
    for(auto& p : score::test::base_interval(*f.doc).processes)
      if(auto sc = qobject_cast<Scenario::ProcessModel*>(&p))
        scenar = sc;
    REQUIRE(scenar);
    auto& event = *scenar->events.begin();
    CommandDispatcher<>{f.ctx().commandStack}.submit<Scenario::Command::SetEventScriptable>(
        event, true);
    settle();
    auto& cond = param(f.ctx(), LocalTree::scriptableAddress(event));

    write(cond, ossia::value{true});
    REQUIRE(cond.value() == ossia::value{true});
    {
      Playback play{*f.doc};
      settle();
      CHECK(cond.value() == ossia::value{false});
      write(cond, ossia::value{true});
      play.stop();
    }
    {
      Playback play{*f.doc};
      settle();
      CHECK(cond.value() == ossia::value{false});
      play.stop();
    }
  });
}
