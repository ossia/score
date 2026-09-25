// The scriptable namespace of the local device: flagged objects become nodes
// under score:/controls, score:/triggers or score:/conditions.

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessState.hpp>

#include <Scenario/Commands/Event/SetCondition.hpp>
#include <Scenario/Commands/Cohesion/RefreshStates.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Commands/State/SnapshotProcess.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/TimeSync/SetAutoTrigger.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/ChangeAddress.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Process/ExecutionFunctions.hpp>
#include <JS/Qml/ScriptableNames.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <LocalTree/ScriptableProcessComponent.hpp>
#include <LocalTree/ScriptableScenarioComponent.hpp>

#include <Process/ExecutionSetup.hpp>
#include <Process/State/MessageNode.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <State/ValueConversion.hpp>

#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>


#include <QQmlEngine>

#include <catch2/catch_all.hpp>
#include <wobjectimpl.h>

namespace
{
//! Exposes the read and write methods of a Device object on the local device.
class TestDevice : public QObject
{
  W_OBJECT(TestDevice)
public:
  explicit TestDevice(score::Document& doc)
      : m_doc{doc}
  {
  }

  QVariant read(const QString& address)
  {
    if(auto p = parameter(address))
      return State::convert::value<QVariant>(p->value());
    return {};
  }
  W_SLOT(read)

  void write(const QString& address, const QVariant& value)
  {
    if(auto p = parameter(address))
      p->push_value(State::convert::fromQVariant(value));
  }
  W_SLOT(write)

private:
  ossia::net::parameter_base* parameter(const QString& address)
  {
    auto addr = State::Address::fromString(address);
    if(!addr)
      return nullptr;
    auto& dev = m_doc.context().plugin<LocalTree::DocumentPlugin>().device();
    auto n = ossia::net::find_node(dev.get_root_node(), addr->path.join('/').toStdString());
    return n ? n->get_parameter() : nullptr;
  }

  score::Document& m_doc;
};

const QString smooth_uuid = QStringLiteral("bf603921-5a48-4aa5-9bc1-48a762be6467");
const QString automation_uuid = QStringLiteral("d2a67bd8-5d3f-404e-b6e9-e350cf2a833f");
const QString js_uuid = QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0");
// A script is what a JS process holds beyond its controls; this one has none
const QString scriptOnly = QStringLiteral("import Score\nScript { tick: function(t, s) { } }");

ossia::net::node_base* local_node(score::Document& doc, const QString& path)
{
  auto& dev = doc.context().plugin<LocalTree::DocumentPlugin>().device();
  return ossia::net::find_node(dev.get_root_node(), path.toStdString());
}

Process::ControlInlet& control(Process::ProcessModel& p, const QString& name)
{
  for(auto inlet : p.inlets())
    if(auto ctl = qobject_cast<Process::ControlInlet*>(inlet); ctl && ctl->name() == name)
      return *ctl;
  FAIL("no control named " << name.toStdString());
  throw;
}

Scenario::ProcessModel& base_scenario(score::Document& doc)
{
  auto& itv = score::test::base_interval(doc);
  for(auto& proc : itv.processes)
    if(auto* s = qobject_cast<Scenario::ProcessModel*>(&proc))
      return *s;
  FAIL("no scenario in the base interval");
  throw;
}

Scenario::StateModel& some_state(score::Document& doc)
{
  auto& scenar = base_scenario(doc);
  REQUIRE(scenar.states.size() > 0);
  return *scenar.states.begin();
}

//! Runs the pending execution commands as the audio thread, which they require.
void run_exec(Execution::DocumentPlugin& plug)
{
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  plug.runAllCommands();
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
}

void settle()
{
  for(int i = 0; i < 8; i++)
  {
    QCoreApplication::sendPostedEvents();
    QApplication::processEvents(QEventLoop::AllEvents, 10);
  }
}
}

TEST_CASE("a scriptable port is published by name, and unpublished by undo")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    REQUIRE(!ctl.scriptable());
    REQUIRE(LocalTree::scriptableAddress(ctl).toString().isEmpty());

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::SetPortScriptable>(ctl, true);

    const auto addr = LocalTree::scriptableAddress(ctl);
    REQUIRE(addr.device == "score");
    REQUIRE(
        addr.path
        == QStringList{"controls", proc->metadata().getName(), ctl.exposed()});
    const auto path = "/" + addr.path.join('/');
    auto node = local_node(*doc, path);
    REQUIRE(node);
    REQUIRE(node->get_parameter());
    REQUIRE(node->get_parameter()->value() == ctl.value());

    // Values sync from device to model and back
    node->get_parameter()->push_value(0.7f);
    settle();
    REQUIRE(ctl.value() == ossia::value{0.7f});
    disp.submit<Process::SetValue>(ctl, ossia::value{0.3f});
    settle();
    REQUIRE(node->get_parameter()->value() == ossia::value{0.3f});

    doc->commandStack().undo();
    doc->commandStack().undo();
    REQUIRE(!ctl.scriptable());
    REQUIRE(!local_node(*doc, path));
    REQUIRE(LocalTree::scriptableAddress(ctl).toString().isEmpty());
  });
}

TEST_CASE("the scripting name follows the model, and a collision is written back")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    std::vector<Process::ControlInlet*> controls;
    for(auto inlet : proc->inlets())
      if(auto ctl = qobject_cast<Process::ControlInlet*>(inlet))
        controls.push_back(ctl);
    REQUIRE(controls.size() >= 2);
    auto& a = *controls[0];
    auto& b = *controls[1];

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::SetPortScriptable>(a, true);
    disp.submit<Process::SetPortScriptable>(b, true);
    disp.submit<Process::SetPortScriptingName>(a, QStringLiteral("wet"));
    const auto procPath = "/controls/" + proc->metadata().getName();
    REQUIRE(local_node(*doc, procPath + "/wet"));
    REQUIRE(a.exposed() == "wet");

    disp.submit<Process::SetPortScriptingName>(b, QStringLiteral("wet"));
    REQUIRE(b.exposed() != "wet");
    REQUIRE(local_node(*doc, procPath + "/" + b.exposed()));

    // Renaming the port keeps an explicitly chosen scripting name
    a.setName(QStringLiteral("Something else"));
    REQUIRE(a.exposed() == "wet");

    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    REQUIRE(LocalTree::scriptableAddress(a).path == QStringList{"controls", "fx", "wet"});
    REQUIRE(local_node(*doc, "/controls/fx/wet"));
  });
}

TEST_CASE("a name given on a collision is taken back when the publication is undone")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};

    // Objects of the same parent get distinct names from the model: the
    // colliding ones are in another interval, or another scenario
    auto& scenar = base_scenario(*doc);
    auto& startState = scenar.states.at(scenar.startEvent().states().front());
    auto create = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        scenar, startState.id(), TimeVal::fromMsecs(2000), 0.5, false};
    disp.submit(create);
    auto& child = scenar.intervals.at(create->createdInterval());
    auto addTo = [&](Scenario::IntervalModel& itv, const UuidKey<Process::ProcessModel>& key,
                     const QString& data) -> Process::ProcessModel* {
      auto cmd = new Scenario::Command::AddOnlyProcessToInterval{itv, key, data, QPointF{}};
      disp.submit(cmd);
      auto it = itv.processes.find(cmd->processId());
      return it != itv.processes.end() ? &*it : nullptr;
    };

    SECTION("processes")
    {
      auto a = score::test::add_process(*doc, smooth_uuid, {});
      auto b = addTo(child, UuidKey<Process::ProcessModel>::fromString(smooth_uuid), {});
      REQUIRE(a);
      REQUIRE(b);
      disp.submit<Process::RenameProcess>(*a, QStringLiteral("fx"));
      disp.submit<Process::RenameProcess>(*b, QStringLiteral("fx"));
      disp.submit<Process::SetProcessScriptable>(*a, true);
      const auto before = stack.currentIndex();

      disp.submit<Process::SetProcessScriptable>(*b, true);
      REQUIRE(b->metadata().getName() == "fx.1");
      REQUIRE(local_node(*doc, "/controls/fx.1"));

      stack.undo();
      REQUIRE(stack.currentIndex() == before);
      REQUIRE(b->metadata().getName() == "fx");
      REQUIRE(a->metadata().getName() == "fx");
      REQUIRE(!local_node(*doc, "/controls/fx.1"));

      stack.redo();
      REQUIRE(b->metadata().getName() == "fx.1");
      REQUIRE(local_node(*doc, "/controls/fx.1"));

      // A name chosen afterwards is kept
      disp.submit<Process::RenameProcess>(*b, QStringLiteral("other"));
      disp.submit<Process::SetProcessScriptable>(*b, false);
      REQUIRE(b->metadata().getName() == "other");
      stack.undo();
      stack.undo();
      REQUIRE(b->metadata().getName() == "fx.1");
      stack.undo();
      REQUIRE(b->metadata().getName() == "fx");
    }

    SECTION("controls, with a chosen scripting name or one following the port name")
    {
      auto proc = score::test::add_process(*doc, smooth_uuid, {});
      REQUIRE(proc);
      std::vector<Process::ControlInlet*> controls;
      for(auto inlet : proc->inlets())
        if(auto ctl = qobject_cast<Process::ControlInlet*>(inlet))
          controls.push_back(ctl);
      REQUIRE(controls.size() >= 2);
      auto& a = *controls[0];
      auto& b = *controls[1];
      disp.submit<Process::SetPortScriptingName>(a, QStringLiteral("wet"));
      disp.submit<Process::SetPortScriptingName>(b, QStringLiteral("wet"));
      disp.submit<Process::SetPortScriptable>(a, true);
      disp.submit<Process::SetPortScriptable>(b, true);
      REQUIRE(b.exposed() == "wet.1");
      stack.undo();
      REQUIRE(b.exposed() == "wet");
      REQUIRE(b.hasOwnExposed());

      // A control named like the state node of its process
      auto js = score::test::add_process(
          *doc, js_uuid,
          QStringLiteral("import Score\nScript { FloatSlider { objectName: \"state\"; init: 0.5 }\n"
                         "tick: function(t, s) { } }"));
      REQUIRE(js);
      auto& state = control(*js, QStringLiteral("state"));
      const bool own = state.hasOwnExposed();
      disp.submit<Process::SetProcessScriptable>(*js, true);
      REQUIRE(state.exposed() == "state.1");
      stack.undo();
      REQUIRE(state.exposed() == "state");
      REQUIRE(state.hasOwnExposed() == own);
    }

    SECTION("syncs and events")
    {
      auto nested = qobject_cast<Scenario::ProcessModel*>(addTo(
          child, Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), {}));
      REQUIRE(nested);
      auto& s1 = scenar.startTimeSync();
      auto& s2 = nested->startTimeSync();
      auto& e1 = scenar.startEvent();
      auto& e2 = nested->startEvent();
      s1.metadata().setName(QStringLiteral("go"));
      s2.metadata().setName(QStringLiteral("go"));
      e1.metadata().setName(QStringLiteral("if"));
      e2.metadata().setName(QStringLiteral("if"));

      disp.submit<Scenario::Command::SetTimeSyncScriptable>(s1, true);
      disp.submit<Scenario::Command::SetTimeSyncScriptable>(s2, true);
      REQUIRE(s2.metadata().getName() == "go.1");
      stack.undo();
      REQUIRE(s2.metadata().getName() == "go");

      disp.submit<Scenario::Command::SetEventScriptable>(e1, true);
      disp.submit<Scenario::Command::SetEventScriptable>(e2, true);
      REQUIRE(e2.metadata().getName() == "if.1");
      stack.undo();
      REQUIRE(e2.metadata().getName() == "if");
    }
  });
}

TEST_CASE("processes of the same name snapshotted into states keep a whole snapshot")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    CommandDispatcher<> disp{dctx.commandStack};
    auto& state = some_state(*doc);
    for(int i = 0; i < 4; i++)
    {
      auto proc = score::test::add_process(*doc, smooth_uuid, {});
      REQUIRE(proc);
      disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
      Scenario::Command::snapshotProcessInState(state, *proc, dctx);
    }
    settle();

    auto snap = dctx.plugin<LocalTree::DocumentPlugin>().snapshot();
    REQUIRE(snap);
    auto& controls = snap->entries.at("controls");
    REQUIRE(controls.children.size() == 4);
    for(auto& [key, entry] : snap->entries)
      for(auto& child : entry.children)
        REQUIRE(snap->entries.count(key + '/' + child) == 1);
  });
}

TEST_CASE("a process unflagged and flagged again is referenced through its new nodes")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    CommandDispatcher<> disp{dctx.commandStack};
    auto proc = score::test::add_process(*doc, js_uuid, scriptOnly);
    REQUIRE(proc);
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("curve"));
    auto& state = some_state(*doc);
    Scenario::Command::snapshotProcessInState(state, *proc, dctx);
    const QStringList path{"controls", "curve", "state"};
    REQUIRE(LocalTree::scriptableStateAddress(*proc).path == path);

    for(int i = 0; i < 3; i++)
    {
      disp.submit<Process::SetProcessScriptable>(*proc, false);
      settle();
      REQUIRE(!local_node(*doc, "/controls/curve"));
      disp.submit<Process::SetProcessScriptable>(*proc, true);
      settle();

      auto& tree = dctx.plugin<LocalTree::DocumentPlugin>();
      REQUIRE(tree.nodeOf(*proc, QStringLiteral("state")) == local_node(*doc, "/controls/curve/state"));
      const auto msgs = Process::flatten(state.messages().rootNode());
      REQUIRE(msgs.size() == 1);
      REQUIRE(msgs[0].address.address.path == path);
      REQUIRE(tree.references().broken().empty());
    }
  });
}

TEST_CASE("a scriptable process gets a state parameter that applies what is beyond its controls")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::SetValue>(ctl, ossia::value{0.2f});
    REQUIRE(Process::stateBeyondControls(*proc).isEmpty());

    disp.submit<Process::SetProcessScriptable>(*proc, true);
    const auto addr = LocalTree::scriptableStateAddress(*proc);
    REQUIRE(addr.path == QStringList{"controls", proc->metadata().getName(), "state"});
    auto node = local_node(*doc, "/" + addr.path.join('/'));
    REQUIRE(node);

    // A script's preset includes its program, which is not a control
    auto js = score::test::add_process(*doc, js_uuid, scriptOnly);
    REQUIRE(js);
    disp.submit<Process::SetProcessScriptable>(*js, true);
    const auto before = Process::stateBeyondControls(*js);
    REQUIRE(!before.isEmpty());
    REQUIRE(before.contains("Data"));
    const auto program = js->savePreset().data;
    auto jnode = local_node(*doc, "/" + LocalTree::scriptableStateAddress(*js).path.join('/'));
    REQUIRE(jnode);
    jnode->get_parameter()->push_value(before.toStdString());
    settle();
    REQUIRE(js->savePreset().data == program);

    // Undoing the flag removes the node
    while(local_node(*doc, "/" + addr.path.join('/')) && doc->commandStack().canUndo())
      doc->commandStack().undo();
    REQUIRE(!local_node(*doc, "/" + addr.path.join('/')));
  });
}

TEST_CASE("flags survive a save and load, and are published again")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::SetProcessScriptable>(*proc, true);
    disp.submit<Process::SetPortScriptable>(ctl, true);
    disp.submit<Process::SetPortScriptingName>(ctl, QStringLiteral("wet"));
    auto& sync = *base_scenario(*doc).timeSyncs.begin();
    disp.submit<Scenario::Command::SetTimeSyncScriptable>(sync, true);
    auto& event = *base_scenario(*doc).events.begin();
    disp.submit<Scenario::Command::SetEventScriptable>(event, true);

    auto reloaded = score::test::reload_via_json(ctx, *doc);
    REQUIRE(reloaded);
    auto& itv = score::test::base_interval(*reloaded);
    Process::ProcessModel* proc2{};
    for(auto& p : itv.processes)
      if(p.concreteKey() == proc->concreteKey())
        proc2 = &p;
    REQUIRE(proc2);
    REQUIRE(proc2->scriptable());
    auto& ctl2 = control(*proc2, QStringLiteral("Amount"));
    REQUIRE(ctl2.scriptable());
    REQUIRE(ctl2.exposed() == "wet");
    REQUIRE(local_node(*reloaded, "/controls/" + proc2->metadata().getName() + "/wet"));
    REQUIRE(local_node(*reloaded, "/controls/" + proc2->metadata().getName() + "/state"));
    REQUIRE(local_node(*reloaded, "/triggers/" + sync.metadata().getName()));
    REQUIRE(local_node(*reloaded, "/conditions/" + event.metadata().getName()));
  });
}

TEST_CASE("a scriptable sync is fired through its impulse")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto& sync = *base_scenario(*doc).timeSyncs.begin();
    int fired = 0;
    QObject::connect(&sync, &Scenario::TimeSyncModel::triggeredByGui, &sync, [&] { fired++; });

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::SetTimeSyncScriptable>(sync, true);
    const auto addr = LocalTree::scriptableAddress(sync);
    REQUIRE(addr.path == QStringList{"triggers", sync.metadata().getName()});
    auto node = local_node(*doc, "/" + addr.path.join('/'));
    REQUIRE(node);
    node->get_parameter()->push_value(ossia::impulse{});
    settle();
    REQUIRE(fired == 1);

    doc->commandStack().undo();
    REQUIRE(!local_node(*doc, "/" + addr.path.join('/')));
  });
}

TEST_CASE("a scriptable event publishes a boolean")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto& event = *base_scenario(*doc).events.begin();
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::SetEventScriptable>(event, true);
    const auto addr = LocalTree::scriptableAddress(event);
    REQUIRE(addr.path == QStringList{"conditions", event.metadata().getName()});
    auto node = local_node(*doc, "/" + addr.path.join('/'));
    REQUIRE(node);
    REQUIRE(node->get_parameter()->get_value_type() == ossia::val_type::BOOL);
  });
}

TEST_CASE("a process is snapshotted into a state as messages to its controls")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& state = some_state(*doc);

    Scenario::Command::snapshotProcessInState(state, *proc, doc->context());
    // The process is published, and its controls with it
    REQUIRE(proc->scriptable());

    int controls = 0;
    proc->forEachControl([&](Process::ControlInlet& ctl, const ossia::value& v) {
      REQUIRE(LocalTree::wantsPublished(ctl));
      const auto addr = LocalTree::scriptableAddress(ctl);
      REQUIRE(addr.isSet());
      const auto msgs = Process::flatten(state.messages().rootNode());
      auto it = ossia::find_if(msgs, [&](auto& m) { return m.address.address == addr; });
      REQUIRE(it != msgs.end());
      REQUIRE(it->value == v);
      controls++;
    });
    REQUIRE(Process::flatten(state.messages().rootNode()).size() == controls);

    // A single undo removes both the flags and the messages
    doc->commandStack().undo();
    proc->forEachControl(
        [&](Process::ControlInlet& ctl, const ossia::value&) { REQUIRE(!ctl.scriptable()); });
    REQUIRE(Process::flatten(state.messages().rootNode()).empty());
  });
}

TEST_CASE("a process with state beyond its controls is snapshotted as its controls and that state")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, js_uuid, scriptOnly);
    REQUIRE(proc);
    auto& state = some_state(*doc);

    Scenario::Command::snapshotProcessInState(state, *proc, doc->context());

    REQUIRE(proc->scriptable());
    const auto msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0].address.address == LocalTree::scriptableStateAddress(*proc));
    auto json = msgs[0].value.target<std::string>();
    REQUIRE(json);
    REQUIRE(*json == Process::stateBeyondControls(*proc).toStdString());
  });
}

TEST_CASE("the local device is on the execution state without being shown")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::SetPortScriptable>(ctl, true);

    auto& explorer = Explorer::deviceExplorerFromContext(doc->context());
    REQUIRE(!Device::try_getNodeFromString(explorer.rootNode(), QStringList{"score"}));

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    run_exec(plug);

    const auto addr = State::AddressAccessor{LocalTree::scriptableAddress(ctl)};
    auto dest = Execution::makeDestination(*plug.context().execState, addr);
    REQUIRE(dest);
    REQUIRE(
        &dest->address()
        == local_node(*doc, "/" + addr.address.path.join('/'))->get_parameter());
  });
}

TEST_CASE("an automation on a published control takes the control's range")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Freq (1e/LP)"));
    CommandDispatcher<> disp{dctx.commandStack};
    disp.submit<Process::SetPortScriptable>(ctl, true);

    auto& explorer = Explorer::deviceExplorerFromContext(dctx);
    REQUIRE(!Device::try_getNodeFromString(explorer.rootNode(), QStringList{"score"}));

    const auto addr = State::AddressAccessor{LocalTree::scriptableAddress(ctl)};
    auto settings = Explorer::makeFullAddressAccessorSettings(addr, dctx, 0., 1., 0.5);
    REQUIRE(settings.domain.get());
    REQUIRE(settings.domain.get().convert_min<double>() == Catch::Approx(0.001));
    REQUIRE(settings.domain.get().convert_max<double>() == Catch::Approx(300.));

    auto autom = qobject_cast<Automation::ProcessModel*>(
        score::test::add_process(*doc, automation_uuid, {}));
    REQUIRE(autom);
    disp.submit<Automation::ChangeAddress>(*autom, addr);
    REQUIRE(autom->address() == addr);
    REQUIRE(autom->min() == Catch::Approx(0.001));
    REQUIRE(autom->max() == Catch::Approx(300.));
  });
}

TEST_CASE("a control of a scriptable process is bound at execution through the process flag")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    run_exec(plug);
    auto& setup = plug.context().setup;
    auto it = setup.inlets.find(&ctl);
    REQUIRE(it != setup.inlets.end());
    ossia::inlet& exec_inlet = *it->second.second;
    REQUIRE(!exec_inlet.address.target<ossia::net::parameter_base*>());

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::SetProcessScriptable>(*proc, true);
    settle();
    run_exec(plug);
    REQUIRE(!ctl.scriptable());
    auto param = local_node(*doc, "/" + LocalTree::scriptableAddress(ctl).path.join('/'))
                     ->get_parameter();
    auto bound = exec_inlet.address.target<ossia::net::parameter_base*>();
    REQUIRE(bound);
    REQUIRE(*bound == param);

    doc->commandStack().undo();
    settle();
    run_exec(plug);
    REQUIRE(!exec_inlet.address.target<ossia::net::parameter_base*>());
  });
}

TEST_CASE("refreshing a state reads published controls and keeps what nothing answers for")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    CommandDispatcher<> disp{dctx.commandStack};
    disp.submit<Process::SetPortScriptable>(ctl, true);
    disp.submit<Process::SetValue>(ctl, ossia::value{0.6f});
    settle();

    auto& state = some_state(*doc);
    const auto published = State::AddressAccessor{LocalTree::scriptableAddress(ctl)};
    disp.submit<Scenario::Command::AddMessagesToState>(
        state, State::MessageList{
                   {published, 0.1f},
                   {*State::parseAddressAccessor("score:/controls/nothing/here"), 0.2f},
                   {*State::parseAddressAccessor("nodevice:/a/b"), 0.3f}});

    Scenario::Command::RefreshStates({&state}, dctx);
    const auto msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs.size() == 3);
    for(auto& m : msgs)
    {
      INFO(m.address.toString().toStdString());
      if(m.address.address == published.address)
        REQUIRE(m.value == ossia::value{0.6f});
      else if(m.address.address.device == "nodevice")
        REQUIRE(m.value == ossia::value{0.3f});
      else
        REQUIRE(m.value == ossia::value{0.2f});
    }
  });
}

TEST_CASE("a scriptable control is bound to its parameter at execution")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::SetPortScriptable>(ctl, true);
    auto param = local_node(*doc, "/" + LocalTree::scriptableAddress(ctl).path.join('/'))
                     ->get_parameter();

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    run_exec(plug);

    auto& setup = plug.context().setup;
    auto it = setup.inlets.find(&ctl);
    REQUIRE(it != setup.inlets.end());
    ossia::inlet& exec_inlet = *it->second.second;
    auto bound = exec_inlet.address.target<ossia::net::parameter_base*>();
    REQUIRE(bound);
    REQUIRE(*bound == param);

    // Clearing the flag during execution unbinds the inlet
    doc->commandStack().undo();
    settle();
    run_exec(plug);
    REQUIRE(!exec_inlet.address.target<ossia::net::parameter_base*>());

    // Setting it again binds to the newly published parameter
    doc->commandStack().redo();
    settle();
    run_exec(plug);
    auto param2 = local_node(*doc, "/" + LocalTree::scriptableAddress(ctl).path.join('/'))
                      ->get_parameter();
    bound = exec_inlet.address.target<ossia::net::parameter_base*>();
    REQUIRE(bound);
    REQUIRE(*bound == param2);
  });
}

TEST_CASE("control messages of an older document become messages to published controls")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));

    // Legacy state format: (port, value) pairs
    JSONReader legacy;
    legacy.stream.StartArray();
    legacy.stream.StartObject();
    legacy.obj["Address"] = Path<Process::Inlet>{ctl};
    legacy.obj["Value"] = ossia::value{0.42f};
    legacy.stream.EndObject();
    legacy.stream.EndArray();

    rapidjson::Document json;
    json.Parse(score::test::save_as_json(*doc).constData());
    REQUIRE(!json.HasParseError());
    rapidjson::Document controls;
    controls.Parse(legacy.toByteArray().constData());
    REQUIRE(!controls.HasParseError());

    // The first state of the document
    bool found = false;
    auto visit = [&](auto& self, rapidjson::Value& v) -> void {
      if(found)
        return;
      if(v.IsObject())
      {
        if(v.HasMember("Messages") && v.HasMember("Event"))
        {
          v.AddMember("Controls", rapidjson::Value{controls, json.GetAllocator()}, json.GetAllocator());
          found = true;
          return;
        }
        for(auto& m : v.GetObject())
          self(self, m.value);
      }
      else if(v.IsArray())
      {
        for(auto& e : v.GetArray())
          self(self, e);
      }
    };
    visit(visit, json);
    REQUIRE(found);

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    json.Accept(writer);

    auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
    auto reloaded = ctx.docManager.loadDocument(
        ctx, QStringLiteral("legacy"), QByteArray{buf.GetString(), (int)buf.GetSize()},
        JSONObject::type(), *delegates.begin());
    REQUIRE(reloaded);
    // Crash backup contents: saved when the document is ready, before the
    // controls are migrated
    const auto backup = reloaded->saveAsByteArray();
    // And a save as JSON before the migration has run
    const auto savedEarly = score::test::save_as_json(*reloaded);
    settle();

    auto& itv = score::test::base_interval(*reloaded);
    Process::ProcessModel* proc2{};
    for(auto& p : itv.processes)
      if(p.concreteKey() == proc->concreteKey())
        proc2 = &p;
    REQUIRE(proc2);
    auto& ctl2 = control(*proc2, QStringLiteral("Amount"));
    REQUIRE(ctl2.scriptable());
    const auto msgs = Process::flatten(some_state(*reloaded).messages().rootNode());
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0].address.address == LocalTree::scriptableAddress(ctl2));
    REQUIRE(msgs[0].value == ossia::value{0.42f});

    // Loading the backup migrates the controls as well
    auto restored = ctx.docManager.loadDocument(
        ctx, QStringLiteral("restored"), backup, DataStream::type(), *delegates.begin());
    REQUIRE(restored);
    settle();
    Process::ProcessModel* proc3{};
    for(auto& p : score::test::base_interval(*restored).processes)
      if(p.concreteKey() == proc->concreteKey())
        proc3 = &p;
    REQUIRE(proc3);
    auto& ctl3 = control(*proc3, QStringLiteral("Amount"));
    REQUIRE(ctl3.scriptable());
    const auto msgs3 = Process::flatten(some_state(*restored).messages().rootNode());
    REQUIRE(msgs3.size() == 1);
    REQUIRE(msgs3[0].address.address == LocalTree::scriptableAddress(ctl3));
    REQUIRE(msgs3[0].value == ossia::value{0.42f});

    // The early save kept the controls to migrate
    auto early = ctx.docManager.loadDocument(
        ctx, QStringLiteral("early"), savedEarly, JSONObject::type(), *delegates.begin());
    REQUIRE(early);
    settle();
    const auto msgs4 = Process::flatten(some_state(*early).messages().rootNode());
    REQUIRE(msgs4.size() == 1);
    REQUIRE(msgs4[0].value == ossia::value{0.42f});
  });
}

W_OBJECT_IMPL(TestDevice)

TEST_CASE("scripts reach published objects by name through Controls and Triggers")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    auto& sync = *base_scenario(*doc).timeSyncs.begin();
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    disp.submit<Process::SetPortScriptable>(ctl, true);
    disp.submit<Process::SetPortScriptingName>(ctl, QStringLiteral("wet"));
    disp.submit<Process::SetValue>(ctl, ossia::value{0.25f});
    disp.submit<Scenario::Command::SetTimeSyncScriptable>(sync, true);
    int fired = 0;
    QObject::connect(&sync, &Scenario::TimeSyncModel::triggeredByGui, &sync, [&] { fired++; });
    settle();

    QQmlEngine engine;
    auto device = engine.newQObject(new TestDevice{*doc});
    auto names = engine.newQObject(new JS::ScriptableNames{nullptr});
    auto ns = JS::ScriptableNames::makeNamespaces(engine, device, names);
    REQUIRE(!ns.isError());
    engine.globalObject().setProperty("Score", ns);

    auto v = engine.evaluate("Score.Controls.fx.wet");
    REQUIRE(!v.isError());
    REQUIRE(v.toNumber() == Catch::Approx(0.25));
    REQUIRE(engine.evaluate("Score.Controls.names()").toVariant().toStringList() == QStringList{"fx"});
    REQUIRE(engine.evaluate("Score.Controls.fx.address()").toString() == "score:/controls/fx");
    REQUIRE(engine.evaluate("'wet' in Score.Controls.fx").toBool());
    REQUIRE(engine.evaluate("Score.Controls.fx.nothing").isUndefined());

    v = engine.evaluate("Score.Controls.fx.wet = 0.75");
    REQUIRE(!v.isError());
    settle();
    REQUIRE(ossia::convert<float>(ctl.value()) == Catch::Approx(0.75f));

    v = engine.evaluate(QStringLiteral("Score.Triggers['%1']()").arg(sync.metadata().getName()));
    REQUIRE(!v.isError());
    settle();
    REQUIRE(fired == 1);
  });
}
