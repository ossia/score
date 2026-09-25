// Shader and script processes snapshotted into states, recalled, edited and reloaded.

#include <State/Address.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Preset.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessState.hpp>
#include <Process/State/MessageNode.hpp>

#include <Gfx/Filter/Process.hpp>
#include <Gfx/ShaderProgram.hpp>
#include <JS/Commands/EditScript.hpp>
#include <JS/JSProcessModel.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <LocalTree/ScriptableProcessComponent.hpp>
#include <LocalTree/ScriptableReference.hpp>
#include <Scenario/Commands/Event/SetCondition.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <State/Expression.hpp>
#include <LocalTree/ReferenceIndex.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Commands/Interval/InsertContentInInterval.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Commands/Scenario/DuplicateInterval.hpp>
#include <Scenario/Commands/Scenario/Encapsulate.hpp>
#include <Scenario/Document/ScenarioEditor.hpp>
#include <Scenario/Application/Drops/DropLayerInInterval.hpp>
#include <Automation/AutomationModel.hpp>
#include <Nodal/Process.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Cohesion/RefreshStatesMacro.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Application/Drops/PresetDrop.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <score/document/DocumentInterface.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Effect/EffectLayer.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <QMimeData>
#include <score/selection/SelectionStack.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Commands/State/SnapshotProcess.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QApplication>
#include <QFile>
#include <QTemporaryDir>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <catch2/catch_all.hpp>

#include <thread>

namespace
{
const QString shader_uuid = QStringLiteral("74ca45ff-92c9-44a0-8f1a-754dea05ee1b");
const QString js_uuid = QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0");

constexpr auto shaderA = R"_(/*{
  "ISFVSN": "2",
  "INPUTS": [
    { "NAME": "scale", "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 10.0 },
    { "NAME": "rate", "TYPE": "float", "DEFAULT": 0.5, "MIN": 0.0, "MAX": 1.0 },
    { "NAME": "center", "TYPE": "point2D", "DEFAULT": [0.0, 0.0] }
  ]
}*/
void main() { gl_FragColor = vec4(scale * rate, center.x, center.y, 1.0); }
)_";

// Second program: "scale" at another position, "gain" added
constexpr auto shaderB = R"_(/*{
  "ISFVSN": "2",
  "INPUTS": [
    { "NAME": "gain", "TYPE": "float", "DEFAULT": 0.25, "MIN": 0.0, "MAX": 1.0 },
    { "NAME": "scale", "TYPE": "float", "DEFAULT": 2.0, "MIN": 0.0, "MAX": 10.0 }
  ]
}*/
void main() { gl_FragColor = vec4(gain * scale); }
)_";

constexpr auto scriptA = R"_(import Score
Script {
  FloatSlider { objectName: "level"; min: 0; max: 10; init: 1 }
  FloatSlider { objectName: "spread"; min: 0; max: 1; init: 0.5 }
  tick: function(token, state) { }
})_";

// "level" moves after another control
constexpr auto scriptA2 = R"_(import Score
Script {
  FloatSlider { objectName: "spread"; min: 0; max: 1; init: 0.5 }
  FloatSlider { objectName: "level"; min: 0; max: 10; init: 1 }
  tick: function(token, state) { }
})_";

// Controls other processes through its outlet addresses
constexpr auto scriptSource = R"_(import Score
Script {
  ValueOutlet { objectName: "toGen" }
  ValueOutlet { objectName: "toOuter" }
  tick: function(token, state) { }
})_";

constexpr auto scriptB = R"_(import Score
Script {
  FloatSlider { objectName: "level"; min: 0; max: 10; init: 3 }
  tick: function(token, state) { }
})_";

void settle()
{
  for(int i = 0; i < 10; i++)
  {
    QCoreApplication::sendPostedEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents(QEventLoop::AllEvents, 10);
  }
}

QString writeShader(const QTemporaryDir& dir, const QString& name, const char* text)
{
  const auto path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(text);
  return path;
}

ossia::net::node_base* node(const score::DocumentContext& ctx, const State::Address& a)
{
  if(!a.isSet())
    return nullptr;
  auto& dev = ctx.plugin<LocalTree::DocumentPlugin>().device();
  auto n = ossia::net::find_node(dev.get_root_node(), a.path.join('/').toStdString());
  return n && !ossia::net::get_zombie(*n) ? n : nullptr;
}

std::vector<Process::ControlInlet*> controls(const Process::ProcessModel& p)
{
  std::vector<Process::ControlInlet*> res;
  for(auto inl : p.inlets())
    if(auto c = qobject_cast<Process::ControlInlet*>(inl))
      res.push_back(c);
  return res;
}

Process::ControlInlet* control(const Process::ProcessModel& p, const QString& name)
{
  for(auto c : controls(p))
    if(c->name() == name)
      return c;
  return nullptr;
}

QStringList controlNames(const Process::ProcessModel& p)
{
  QStringList res;
  for(auto c : controls(p))
    res.push_back(c->name());
  return res;
}

Scenario::StateModel& some_state(score::Document& doc, int index = 0)
{
  auto& itv = score::test::base_interval(doc);
  for(auto& proc : itv.processes)
    if(auto s = qobject_cast<Scenario::ProcessModel*>(&proc))
    {
      REQUIRE(int(s->states.size()) > index);
      auto it = s->states.begin();
      std::advance(it, index);
      return *it;
    }
  FAIL("no scenario");
  throw;
}

State::MessageList messages(const Scenario::StateModel& s)
{
  return Process::flatten(s.messages().rootNode());
}

// Every control is published, at an address with a live node
void requirePublished(const score::DocumentContext& ctx, const Process::ProcessModel& p)
{
  for(auto c : controls(p))
  {
    INFO("control " << c->name().toStdString());
    REQUIRE(LocalTree::wantsPublished(*c));
    const auto addr = LocalTree::scriptableAddress(*c);
    REQUIRE(addr.isSet());
    REQUIRE(node(ctx, addr));
  }
}

// Every message targets a published address; an anchored message targets
// its anchor's address
void requireResolves(const score::DocumentContext& ctx, const Scenario::StateModel& s)
{
  for(auto& m : messages(s))
  {
    const auto& a = m.address.address;
    INFO("message " << a.toString().toStdString());
    REQUIRE(!LocalTree::broken(a, ctx));
    REQUIRE(a.anchored());
    auto target = a.anchor->resolve(ctx);
    REQUIRE(target);
    if(a.anchor->member == QStringLiteral("state"))
    {
      auto proc = qobject_cast<Process::ProcessModel*>(target);
      REQUIRE(proc);
      REQUIRE(LocalTree::scriptableStateAddress(*proc) == a);
    }
    else
    {
      auto port = qobject_cast<Process::Port*>(target);
      REQUIRE(port);
      REQUIRE(LocalTree::scriptableAddress(*port) == a);
    }
  }
}

// Pushes the messages into the published parameters from another thread,
// like the execution does
void recall(const score::DocumentContext& ctx, const State::MessageList& msgs)
{
  std::vector<std::pair<ossia::net::parameter_base*, ossia::value>> targets;
  for(auto& m : msgs)
    if(auto n = node(ctx, m.address.address))
      targets.emplace_back(n->get_parameter(), m.value);
  std::thread{[&] {
    for(auto& [p, v] : targets)
      p->push_value(v);
  }}.join();
  settle();
}

State::Message stateMessage(const Scenario::StateModel& s)
{
  for(auto& m : messages(s))
    if(m.address.address.path.endsWith(QStringLiteral("state")))
      return m;
  FAIL("no state message");
  throw;
}

Gfx::Filter::Model& addShader(score::Document& doc, const QString& path)
{
  auto p = qobject_cast<Gfx::Filter::Model*>(score::test::add_process(doc, shader_uuid, path));
  REQUIRE(p);
  REQUIRE(controlNames(*p) == QStringList{"scale", "rate", "center"});
  return *p;
}
}

TEST_CASE(
    "a shader snapshotted into a state publishes every control and its state",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    QTemporaryDir dir;
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& shader = addShader(*doc, writeShader(dir, "a.fs", shaderA));
    CommandDispatcher<>{ctx.commandStack}.submit<Process::RenameProcess>(
        shader, QStringLiteral("blob"));
    auto& state = some_state(*doc);

    Scenario::Command::snapshotProcessInState(state, shader, ctx);
    settle();

    REQUIRE(shader.scriptable());
    requirePublished(ctx, shader);
    const auto msgs = messages(state);
    REQUIRE(msgs.size() == controls(shader).size() + 1);
    REQUIRE(node(ctx, LocalTree::scriptableStateAddress(shader)));
    requireResolves(ctx, state);
    REQUIRE(ctx.plugin<LocalTree::DocumentPlugin>().references().broken().empty());

    // One undo takes the whole snapshot back
    doc->commandStack().undo();
    settle();
    REQUIRE(messages(state).empty());
    REQUIRE(!shader.scriptable());
    REQUIRE(!node(ctx, State::Address{"score", {"controls", "blob"}}));
  });
}

TEST_CASE(
    "recalling the state a shader is in keeps its ports and sets its controls",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    QTemporaryDir dir;
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& shader = addShader(*doc, writeShader(dir, "a.fs", shaderA));
    auto& state = some_state(*doc);
    CommandDispatcher<> disp{ctx.commandStack};
    disp.submit<Process::SetValue>(*control(shader, "scale"), ossia::value{4.f});
    Scenario::Command::snapshotProcessInState(state, shader, ctx);
    settle();

    const auto before = shader.inlets();
    disp.submit<Process::SetValue>(*control(shader, "scale"), ossia::value{9.f});

    // Recall with every message order the execution may use
    auto msgs = messages(state);
    recall(ctx, msgs);
    REQUIRE(shader.inlets() == before);
    REQUIRE(control(shader, "scale")->value() == ossia::value{4.f});

    disp.submit<Process::SetValue>(*control(shader, "scale"), ossia::value{9.f});
    std::reverse(msgs.begin(), msgs.end());
    recall(ctx, msgs);
    REQUIRE(shader.inlets() == before);
    REQUIRE(control(shader, "scale")->value() == ossia::value{4.f});

    requirePublished(ctx, shader);
    requireResolves(ctx, state);
    REQUIRE(ctx.plugin<LocalTree::DocumentPlugin>().references().broken().empty());
  });
}

TEST_CASE(
    "editing a shader's source keeps its controls published and their scripting names",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    QTemporaryDir dir;
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& shader = addShader(*doc, writeShader(dir, "a.fs", shaderA));
    auto& state = some_state(*doc);
    CommandDispatcher<> disp{ctx.commandStack};
    disp.submit<Process::RenameProcess>(shader, QStringLiteral("blob"));
    Scenario::Command::snapshotProcessInState(state, shader, ctx);
    disp.submit<Process::SetPortScriptingName>(
        *control(shader, "scale"), QStringLiteral("zoom"));
    settle();
    REQUIRE(LocalTree::scriptableAddress(*control(shader, "scale")).path
            == QStringList{"controls", "blob", "zoom"});

    disp.submit(new Gfx::ChangeShader{
        shader,
        Gfx::programFromISFFragmentShaderPath(writeShader(dir, "b.fs", shaderB), {}),
        ctx});
    settle();

    REQUIRE(controlNames(shader) == QStringList{"gain", "scale"});
    requirePublished(ctx, shader);
    REQUIRE(control(shader, "scale")->exposed() == QStringLiteral("zoom"));
    REQUIRE(LocalTree::scriptableAddress(*control(shader, "scale")).path
            == QStringList{"controls", "blob", "zoom"});
    REQUIRE(!node(ctx, State::Address{"score", {"controls", "blob", "rate"}}));

    // Exactly the messages for the removed controls are broken
    auto& index = ctx.plugin<LocalTree::DocumentPlugin>().references();
    QStringList broken;
    for(auto& b : index.broken())
      broken.push_back(b.address.path.last());
    broken.sort();
    REQUIRE(broken == QStringList{"center", "rate"});

    doc->commandStack().undo();
    settle();
    REQUIRE(controlNames(shader) == QStringList{"scale", "rate", "center"});
    requirePublished(ctx, shader);
    requireResolves(ctx, state);
    REQUIRE(index.broken().empty());
  });
}

TEST_CASE(
    "recalling a state holding another shader source brings it back whole",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    QTemporaryDir dir;
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& shader = addShader(*doc, writeShader(dir, "a.fs", shaderA));
    CommandDispatcher<> disp{ctx.commandStack};
    disp.submit<Process::RenameProcess>(shader, QStringLiteral("blob"));
    auto& stateA = some_state(*doc, 0);
    disp.submit<Process::SetValue>(*control(shader, "rate"), ossia::value{0.75f});
    Scenario::Command::snapshotProcessInState(stateA, shader, ctx);

    disp.submit(new Gfx::ChangeShader{
        shader,
        Gfx::programFromISFFragmentShaderPath(writeShader(dir, "b.fs", shaderB), {}),
        ctx});
    settle();
    REQUIRE(controlNames(shader) == QStringList{"gain", "scale"});
    disp.submit<Process::SetValue>(*control(shader, "gain"), ossia::value{0.5f});
    const int edits = doc->commandStack().currentIndex();

    // A single recall restores shader A and its values
    recall(ctx, messages(stateA));
    REQUIRE(controlNames(shader) == QStringList{"scale", "rate", "center"});
    REQUIRE(shader.fragment().contains("scale * rate"));
    REQUIRE(control(shader, "rate")->value() == ossia::value{0.75f});
    requirePublished(ctx, shader);
    requireResolves(ctx, stateA);
    auto& index = ctx.plugin<LocalTree::DocumentPlugin>().references();
    REQUIRE(index.broken().empty());

    // The recall is undoable, and earlier commands still apply after undoing it
    REQUIRE(doc->commandStack().currentIndex() > edits);
    while(doc->commandStack().currentIndex() > edits)
      doc->commandStack().undo();
    settle();
    REQUIRE(controlNames(shader) == QStringList{"gain", "scale"});
    doc->commandStack().undo();
    settle();
    REQUIRE(control(shader, "gain")->value() == ossia::value{0.25f});
    doc->commandStack().redo();
    while(doc->commandStack().canRedo())
      doc->commandStack().redo();
    settle();
    REQUIRE(controlNames(shader) == QStringList{"scale", "rate", "center"});
    REQUIRE(control(shader, "rate")->value() == ossia::value{0.75f});

    // Renaming a control updates the messages anchored to it
    disp.submit<Process::SetPortScriptingName>(
        *control(shader, "rate"), QStringLiteral("speed"));
    settle();
    QStringList names;
    for(auto& m : messages(stateA))
      names.push_back(m.address.address.path.last());
    names.sort();
    REQUIRE(names == QStringList{"center", "scale", "speed", "state"});
    requireResolves(ctx, stateA);
  });
}

TEST_CASE(
    "a document with a shader snapshot reloads published and resolved",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    QTemporaryDir dir;
    auto doc = score::test::new_document(app);
    auto& shader = addShader(*doc, writeShader(dir, "a.fs", shaderA));
    CommandDispatcher<>{doc->context().commandStack}.submit<Process::RenameProcess>(
        shader, QStringLiteral("blob"));
    Scenario::Command::snapshotProcessInState(some_state(*doc), shader, doc->context());
    settle();

    auto reloaded = score::test::reload_via_json(app, *doc);
    REQUIRE(reloaded);
    settle();
    const auto& ctx = reloaded->context();
    Process::ProcessModel* shader2{};
    for(auto& p : score::test::base_interval(*reloaded).processes)
      if(qobject_cast<Gfx::Filter::Model*>(&p))
        shader2 = &p;
    REQUIRE(shader2);
    REQUIRE(shader2->scriptable());
    requirePublished(ctx, *shader2);
    requireResolves(ctx, some_state(*reloaded));
    REQUIRE(ctx.plugin<LocalTree::DocumentPlugin>().references().broken().empty());

    // Recalling the state after reload changes nothing
    const auto before = shader2->inlets();
    recall(ctx, messages(some_state(*reloaded)));
    REQUIRE(shader2->inlets() == before);
    requireResolves(ctx, some_state(*reloaded));
  });
}

TEST_CASE(
    "a script's controls stay published across a script edit and a preset load",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto js = qobject_cast<JS::ProcessModel*>(score::test::add_process(*doc, js_uuid, {}));
    REQUIRE(js);
    CommandDispatcher<> disp{ctx.commandStack};
    disp.submit(new JS::EditScript{*js, JS::QmlSource{scriptA, {}}, ctx});
    settle();
    REQUIRE(controlNames(*js) == QStringList{"level", "spread"});
    const Process::Preset presetA
        = static_cast<const Process::ProcessModel&>(*js).savePreset();
    disp.submit<Process::RenameProcess>(*js, QStringLiteral("gen"));
    auto& state = some_state(*doc);
    Scenario::Command::snapshotProcessInState(state, *js, ctx);
    disp.submit<Process::SetPortScriptingName>(*control(*js, "level"), QStringLiteral("lvl"));
    settle();
    REQUIRE(!Process::stateBeyondControls(*js).isEmpty());
    requirePublished(ctx, *js);
    requireResolves(ctx, state);

    // After a script change, "level" keeps its name and publication
    disp.submit(new JS::EditScript{*js, JS::QmlSource{scriptB, {}}, ctx});
    settle();
    REQUIRE(controlNames(*js) == QStringList{"level"});
    requirePublished(ctx, *js);
    REQUIRE(control(*js, "level")->exposed() == QStringLiteral("lvl"));

    // Loading a preset restores script A, through the UI's command
    auto& factories = ctx.app.interfaces<Process::LoadPresetCommandFactoryList>();
    auto cmd = factories.make(&Process::LoadPresetCommandFactory::make, *js, presetA, ctx);
    REQUIRE(cmd);
    disp.submit(cmd);
    settle();
    REQUIRE(controlNames(*js) == QStringList{"level", "spread"});
    requirePublished(ctx, *js);
    REQUIRE(control(*js, "level")->exposed() == QStringLiteral("lvl"));
    requireResolves(ctx, state);

    // Recalling the state restores it too
    disp.submit(new JS::EditScript{*js, JS::QmlSource{scriptB, {}}, ctx});
    settle();
    recall(ctx, messages(state));
    REQUIRE(controlNames(*js) == QStringList{"level", "spread"});
    requirePublished(ctx, *js);
    requireResolves(ctx, state);
    REQUIRE(ctx.plugin<LocalTree::DocumentPlugin>().references().broken().empty());
  });
}

TEST_CASE(
    "recalling a state holding another script brings it back whole",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto js = qobject_cast<JS::ProcessModel*>(score::test::add_process(*doc, js_uuid, {}));
    REQUIRE(js);
    CommandDispatcher<> disp{ctx.commandStack};
    disp.submit(new JS::EditScript{*js, JS::QmlSource{scriptA, {}}, ctx});
    disp.submit<Process::RenameProcess>(*js, QStringLiteral("seq"));
    disp.submit<Process::SetValue>(*control(*js, "level"), ossia::value{4.f});
    const JS::JSState steps{{QStringLiteral("step"), ossia::value{3}}};
    js->setState(steps);
    auto& state = some_state(*doc);
    Scenario::Command::snapshotProcessInState(state, *js, ctx);

    disp.submit(new JS::EditScript{*js, JS::QmlSource{scriptB, {}}, ctx});
    js->setState({});
    settle();
    REQUIRE(!control(*js, "spread"));

    recall(ctx, messages(state));
    settle();
    REQUIRE(js->program() == JS::QmlSource{scriptA, {}});
    REQUIRE(control(*js, "spread"));
    REQUIRE(control(*js, "level")->value() == ossia::value{4.f});
    REQUIRE(js->state() == steps);
    requirePublished(ctx, *js);
    requireResolves(ctx, state);
    REQUIRE(ctx.plugin<LocalTree::DocumentPlugin>().references().broken().empty());
  });
}

TEST_CASE(
    "a script's own state is snapshotted, recalled and reloaded with its controls",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto js = qobject_cast<JS::ProcessModel*>(score::test::add_process(*doc, js_uuid, {}));
    REQUIRE(js);
    CommandDispatcher<> disp{ctx.commandStack};
    disp.submit(new JS::EditScript{*js, JS::QmlSource{scriptA, {}}, ctx});
    disp.submit<Process::RenameProcess>(*js, QStringLiteral("seq"));
    const JS::JSState steps{
        {QStringLiteral("step"), ossia::value{3}},
        {QStringLiteral("pattern"), ossia::value{std::string{"x..x"}}}};
    js->setState(steps);
    auto& state = some_state(*doc);
    Scenario::Command::snapshotProcessInState(state, *js, ctx);
    settle();
    const auto snapshot = stateMessage(state);
    REQUIRE(snapshot.value.target<std::string>()->find("x..x") != std::string::npos);

    // Only the script state differs: it is restored, the ports are unchanged
    const auto before = js->inlets();
    js->setState(JS::JSState{{QStringLiteral("step"), ossia::value{0}}});
    recall(ctx, messages(state));
    REQUIRE(js->state() == steps);
    REQUIRE(js->inlets() == before);
    requirePublished(ctx, *js);
    requireResolves(ctx, state);

    // Recalling the current state does nothing
    recall(ctx, messages(state));
    REQUIRE(js->inlets() == before);
    REQUIRE(js->state() == steps);

    // After a save and reload too
    js->setState({});
    auto reloaded = score::test::reload_via_json(app, *doc);
    REQUIRE(reloaded);
    settle();
    JS::ProcessModel* js2{};
    for(auto& p : score::test::base_interval(*reloaded).processes)
      if(auto j = qobject_cast<JS::ProcessModel*>(&p))
        js2 = j;
    REQUIRE(js2);
    REQUIRE(js2->state().empty());
    recall(reloaded->context(), messages(some_state(*reloaded)));
    REQUIRE(js2->state() == steps);
    requireResolves(reloaded->context(), some_state(*reloaded));
  });
}

TEST_CASE(
    "a condition on a script control settles when the script moves its ports",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto js = qobject_cast<JS::ProcessModel*>(score::test::add_process(*doc, js_uuid, {}));
    REQUIRE(js);
    CommandDispatcher<> disp{ctx.commandStack};
    disp.submit(new JS::EditScript{*js, JS::QmlSource{scriptB, {}}, ctx});
    disp.submit<Process::RenameProcess>(*js, QStringLiteral("gen"));
    disp.submit<Process::SetProcessScriptable>(*js, true);
    settle();

    auto& itv = score::test::base_interval(*doc);
    Scenario::ProcessModel* scenar{};
    for(auto& p : itv.processes)
      if(auto sc = qobject_cast<Scenario::ProcessModel*>(&p))
        scenar = sc;
    REQUIRE(scenar);
    auto& event = scenar->event(some_state(*doc).eventId());
    disp.submit<Scenario::Command::SetCondition>(
        event, *State::parseExpression(QStringLiteral("%score:/controls/gen/level% > 2")));
    REQUIRE(State::anchors(event.condition())[0]);

    auto& index = ctx.plugin<LocalTree::DocumentPlugin>().references();
    int changes = 0;
    auto counting = QObject::connect(
        &index, &LocalTree::ReferenceIndex::changed, &index, [&] { changes++; });

    // A control inserted before "level": ids change, the name is kept
    disp.submit(new JS::EditScript{*js, JS::QmlSource{scriptA2, {}}, ctx});
    settle();
    settle();
    REQUIRE(changes < 50);
    const int settled = changes;
    settle();
    REQUIRE(changes == settled);

    REQUIRE(event.condition().toString().contains("score:/controls/gen/level"));
    auto target = State::anchors(event.condition())[0]->resolve(ctx);
    REQUIRE(qobject_cast<Process::Port*>(target));
    REQUIRE(qobject_cast<Process::Port*>(target)->name() == QStringLiteral("level"));
    REQUIRE(index.broken().empty());
    QObject::disconnect(counting);
  });
}

TEST_CASE(
    "a control published with its process goes with the process flag",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    QTemporaryDir dir;
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& shader = addShader(*doc, writeShader(dir, "a.fs", shaderA));
    CommandDispatcher<> disp{ctx.commandStack};
    disp.submit<Process::RenameProcess>(shader, QStringLiteral("blob"));
    disp.submit<Process::SetPortScriptable>(*control(shader, "scale"), true);
    disp.submit<Process::SetProcessScriptable>(shader, true);
    settle();
    requirePublished(ctx, shader);
    REQUIRE(LocalTree::publishedWithProcess(*control(shader, "rate")));
    REQUIRE(!control(shader, "rate")->scriptable());

    disp.submit<Process::SetProcessScriptable>(shader, false);
    settle();
    REQUIRE(node(ctx, State::Address{"score", {"controls", "blob", "scale"}}));
    REQUIRE(!node(ctx, State::Address{"score", {"controls", "blob", "rate"}}));
    REQUIRE(!node(ctx, State::Address{"score", {"controls", "blob", "state"}}));
  });
}

TEST_CASE(
    "port data carries the publication across kinds of port",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    Process::ValueInlet value{"x", Id<Process::Port>{0}, nullptr};
    value.setScriptable(true);
    value.setExposed(QStringLiteral("ex"));
    Process::ControlInlet ctl{"x", Id<Process::Port>{1}, nullptr};
    ctl.loadData(value.saveData());
    REQUIRE(ctl.scriptable());
    REQUIRE(ctl.exposed() == QStringLiteral("ex"));

    ctl.setValue(ossia::value{2.f});
    Process::ControlInlet other{"x", Id<Process::Port>{2}, nullptr};
    other.loadData(ctl.saveData(), Process::PortLoadDataFlags::ReloadValue);
    REQUIRE(other.scriptable());
    REQUIRE(other.value() == ossia::value{2.f});
  });
}

namespace
{
Scenario::ProcessModel& scenarioOf(score::Document& doc)
{
  for(auto& p : score::test::base_interval(doc).processes)
    if(auto sc = qobject_cast<Scenario::ProcessModel*>(&p))
      return *sc;
  FAIL("no scenario");
  throw;
}

JS::ProcessModel& addScript(
    const score::DocumentContext& ctx, Scenario::IntervalModel& itv, const QString& name,
    const char* script)
{
  CommandDispatcher<> disp{ctx.commandStack};
  auto cmd = new Scenario::Command::AddOnlyProcessToInterval{
      itv, UuidKey<Process::ProcessModel>::fromString(js_uuid), {}, QPointF{}};
  disp.submit(cmd);
  auto js = qobject_cast<JS::ProcessModel*>(&itv.processes.at(cmd->processId()));
  REQUIRE(js);
  disp.submit(new JS::EditScript{*js, JS::QmlSource{script, {}}, ctx});
  disp.submit<Process::RenameProcess>(*js, name);
  disp.submit<Process::SetProcessScriptable>(*js, true);
  return *js;
}

const Process::Port* anchoredTo(const State::Message& m, const score::DocumentContext& ctx)
{
  if(!m.address.address.anchor)
    return nullptr;
  return qobject_cast<Process::Port*>(m.address.address.anchor->resolve(ctx));
}
}

TEST_CASE(
    "a pasted copy refers to its own copies, and to the rest as it did",
    "[integration][scriptable][paste]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    CommandDispatcher<> disp{ctx.commandStack};
    auto& scenar = scenarioOf(*doc);
    auto create = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        scenar, scenar.states.begin()->id(), TimeVal::fromMsecs(2000), 0.3, false};
    const auto itvId = create->createdInterval();
    disp.submit(create);
    auto& itv = scenar.interval(itvId);
    auto& gen = addScript(ctx, itv, QStringLiteral("gen"), scriptA);
    auto& outer = addScript(ctx, score::test::base_interval(*doc), QStringLiteral("outer"), scriptB);
    settle();
    auto& end = scenar.state(itv.endState());
    const auto genAddr = LocalTree::scriptableAddress(*control(gen, "level"));
    const auto outerAddr = LocalTree::scriptableAddress(*control(outer, "level"));
    disp.submit<Scenario::Command::AddMessagesToState>(
        end, State::MessageList{{State::AddressAccessor{genAddr}, 2.f},
                                {State::AddressAccessor{outerAddr}, 3.f}});

    JSONReader r;
    Scenario::copyWholeScenario(r, scenar);
    const auto json = readJson(r.toByteArray());

    auto check = [&](score::Document& target, bool sameDocument) {
      const auto& tctx = target.context();
      auto& tscenar = scenarioOf(target);
      const auto before = tscenar.intervals.size();
      CommandDispatcher<>{tctx.commandStack}.submit(new Scenario::Command::ScenarioPasteElements{
          tscenar, json, Scenario::Point{TimeVal::fromMsecs(5000), 0.6}});
      settle();
      REQUIRE(tscenar.intervals.size() > before);

      // Find the pasted interval holding the copy of "gen"
      JS::ProcessModel* copy{};
      Scenario::IntervalModel* pastedItv{};
      for(auto& i : tscenar.intervals)
        for(auto& p : i.processes)
          if(auto js = qobject_cast<JS::ProcessModel*>(&p); js && js != &gen)
          {
            copy = js;
            pastedItv = &i;
          }
      REQUIRE(copy);
      auto& pastedEnd = tscenar.state(pastedItv->endState());
      const auto copyAddr = LocalTree::scriptableAddress(*control(*copy, "level"));
      REQUIRE(copyAddr.isSet());
      if(sameDocument)
        REQUIRE(copyAddr != genAddr);

      bool sawCopy = false, sawOuter = false;
      for(auto& m : messages(pastedEnd))
      {
        if(m.value == ossia::value{2.f})
        {
          REQUIRE(anchoredTo(m, tctx) == control(*copy, "level"));
          REQUIRE(m.address.address == copyAddr);
          sawCopy = true;
        }
        else
        {
          if(sameDocument)
          {
            REQUIRE(anchoredTo(m, tctx) == control(outer, "level"));
            REQUIRE(m.address.address == outerAddr);
          }
          else
          {
            REQUIRE(!m.address.address.anchored());
          }
          sawOuter = true;
        }
      }
      REQUIRE(sawCopy);
      REQUIRE(sawOuter);
    };

    check(*doc, true);
    // The original is unchanged
    for(auto& m : messages(end))
      REQUIRE(anchoredTo(m, ctx) == (m.value == ossia::value{2.f} ? control(gen, "level")
                                                                   : control(outer, "level")));

    auto other = score::test::new_document(app);
    check(*other, false);
  });
}

TEST_CASE(
    "pasted and duplicated processes drive their own copies, and the rest as they did",
    "[integration][scriptable][paste]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    CommandDispatcher<> disp{ctx.commandStack};
    auto& scenar = scenarioOf(*doc);
    auto create = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        scenar, scenar.states.begin()->id(), TimeVal::fromMsecs(2000), 0.3, false};
    const auto itvId = create->createdInterval();
    disp.submit(create);
    auto& itv = scenar.interval(itvId);
    auto& gen = addScript(ctx, itv, QStringLiteral("gen"), scriptA);
    auto& src = addScript(ctx, itv, QStringLiteral("src"), scriptSource);
    auto& outer
        = addScript(ctx, score::test::base_interval(*doc), QStringLiteral("outer"), scriptB);
    settle();
    const auto genAddr = LocalTree::scriptableAddress(*control(gen, "level"));
    const auto outerAddr = LocalTree::scriptableAddress(*control(outer, "level"));
    REQUIRE(src.outlets().size() == 2);
    disp.submit(new Process::ChangePortAddress{
        *src.outlets()[0], State::AddressAccessor{genAddr}});
    disp.submit(new Process::ChangePortAddress{
        *src.outlets()[1], State::AddressAccessor{outerAddr}});
    settle();

    auto target = [](const Process::Port& port, const score::DocumentContext& c) {
      const auto& anchor = port.address().address.anchor;
      return anchor ? qobject_cast<Process::Port*>(anchor->resolve(c)) : nullptr;
    };
    REQUIRE(target(*src.outlets()[0], ctx) == control(gen, "level"));
    REQUIRE(target(*src.outlets()[1], ctx) == control(outer, "level"));

    ctx.selectionStack.pushNewSelection(Selection{&src, &gen});
    JSONReader r;
    REQUIRE(Scenario::copySelectedProcesses(r, ctx));
    const auto copied = r.toByteArray();

    std::string phase;
    auto check = [&](const Scenario::IntervalModel& at, const score::DocumentContext& c,
                     bool sameDocument) {
      const Process::ProcessModel* g{};
      const Process::ProcessModel* s{};
      for(auto& p : at.processes)
        (control(p, "level") ? g : s) = &p;
      REQUIRE(g);
      REQUIRE(s);
      REQUIRE(g != &gen);
      REQUIRE(s->outlets().size() == 2);
      const auto gAddr = LocalTree::scriptableAddress(*control(*g, "level"));
      REQUIRE(gAddr.isSet());
      INFO(phase);
      if(sameDocument)
        REQUIRE(gAddr != genAddr);

      const auto& toGen = *s->outlets()[0];
      REQUIRE(target(toGen, c) == control(*g, "level"));
      REQUIRE(toGen.address().address == gAddr);

      const auto& toOuter = *s->outlets()[1];
      if(sameDocument)
      {
        REQUIRE(target(toOuter, c) == control(outer, "level"));
        REQUIRE(toOuter.address().address == outerAddr);
      }
      else
      {
        REQUIRE(!toOuter.address().address.anchored());
      }
    };
    auto undoRedo = [](score::Document& d) {
      d.commandStack().undo();
      settle();
      d.commandStack().redo();
      settle();
    };
    auto newInterval = [](const Scenario::ProcessModel& sc, const auto& before)
        -> const Scenario::IntervalModel& {
      for(auto& i : sc.intervals)
        if(std::find(before.begin(), before.end(), i.id()) == before.end())
          return i;
      FAIL("no new interval");
      throw;
    };
    auto ids = [](const Scenario::ProcessModel& sc) {
      std::vector<Id<Scenario::IntervalModel>> res;
      for(auto& i : sc.intervals)
        res.push_back(i.id());
      return res;
    };

    // In an interval
    {
      phase = "In an interval";
      auto create2 = new Scenario::Command::CreateInterval_State_Event_TimeSync{
          scenar, itv.endState(), TimeVal::fromMsecs(3000), 0.7, false};
      const auto itv2Id = create2->createdInterval();
      disp.submit(create2);
      auto& itv2 = scenar.interval(itv2Id);
      auto json = readJson(copied);
      disp.submit(new Scenario::Command::PasteProcessesInInterval{
          json, itv2, ExpandMode::GrowShrink, QPointF{}});
      settle();
      check(itv2, ctx, true);
      undoRedo(*doc);
      check(scenar.interval(itv2Id), ctx, true);
    }

    // In a new box
    {
      phase = "In a new box";
      const auto before = ids(scenar);
      auto json = readJson(copied);
      REQUIRE(Scenario::pasteProcessesInNewBox(
          scenar, Scenario::Point{TimeVal::fromMsecs(8000), 0.8}, json, ctx));
      settle();
      check(newInterval(scenar, before), ctx, true);
      undoRedo(*doc);
      check(newInterval(scenar, before), ctx, true);
    }

    // Duplicated with their interval
    {
      phase = "Duplicated with their interval";
      auto dup = new Scenario::Command::DuplicateInterval{scenar, itv};
      const auto dupId = dup->createdId();
      disp.submit(dup);
      settle();
      check(scenar.interval(dupId), ctx, true);
      undoRedo(*doc);
      check(scenar.interval(dupId), ctx, true);
    }

    // In another document
    {
      phase = "In another document";
      auto other = score::test::new_document(app);
      auto& oscenar = scenarioOf(*other);
      const auto before = ids(oscenar);
      auto json = readJson(copied);
      REQUIRE(Scenario::pasteProcessesInNewBox(
          oscenar, Scenario::Point{TimeVal::fromMsecs(1000), 0.5}, json,
          other->context()));
      settle();
      check(newInterval(oscenar, before), other->context(), false);
    }
  });
}

TEST_CASE(
    "the preset button of one of several selected processes snapshots them all in a state",
    "[integration][scriptable][snapshot]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& base = score::test::base_interval(*doc);
    auto& a = addScript(ctx, base, QStringLiteral("alpha"), scriptA);
    auto& b = addScript(ctx, base, QStringLiteral("beta"), scriptB);
    auto& c = addScript(ctx, base, QStringLiteral("gamma"), scriptB);
    settle();
    auto& state = *scenarioOf(*doc).states.begin();

    auto drag = [&](const Process::ProcessModel& from) {
      QObject owner;
      std::unique_ptr<score::QGraphicsDraggablePixmap> button{
          Process::makePresetButton(from, ctx, &owner, nullptr)};
      auto mime = std::make_unique<QMimeData>();
      button->createDrag(*mime);
      return mime;
    };
    auto drop = [&](const QMimeData& mime) {
      Scenario::StatePresenter presenter{state, ctx, nullptr, nullptr};
      presenter.handleDrop(mime);
    };
    auto addressesIn = [&] {
      std::vector<State::Address> res;
      for(auto& m : messages(state))
        res.push_back(m.address.address);
      return res;
    };
    auto has = [&](const Process::ProcessModel& p, const char* control_name) {
      const auto addr = LocalTree::scriptableAddress(*control(p, control_name));
      return ossia::contains(addressesIn(), addr);
    };

    // Several selected: all are snapshotted, in one command
    ctx.selectionStack.pushNewSelection(Selection{&a, &b});
    const int before = doc->commandStack().currentIndex();
    drop(*drag(a));
    REQUIRE(doc->commandStack().currentIndex() == before + 1);
    REQUIRE(has(a, "level"));
    REQUIRE(has(a, "spread"));
    REQUIRE(has(b, "level"));
    REQUIRE(!has(c, "level"));

    doc->commandStack().undo();
    REQUIRE(messages(state).empty());

    // Dragging a process outside the selection snapshots only that process
    ctx.selectionStack.pushNewSelection(Selection{&a, &b});
    drop(*drag(c));
    REQUIRE(has(c, "level"));
    REQUIRE(!has(a, "level"));
    REQUIRE(!has(b, "level"));
  });
}

TEST_CASE(
    "the preset button makes a cue when dropped in a scenario, and a copy in an interval",
    "[integration][scriptable][snapshot]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& base = score::test::base_interval(*doc);
    auto& a = addScript(ctx, base, QStringLiteral("alpha"), scriptA);
    auto& b = addScript(ctx, base, QStringLiteral("beta"), scriptB);
    CommandDispatcher<>{ctx.commandStack}.submit<Process::SetValue>(
        *control(a, "level"), ossia::value{7.f});
    settle();
    auto& scenar = scenarioOf(*doc);

    ctx.selectionStack.pushNewSelection(Selection{&a, &b});
    auto drag = [&] {
      QObject owner;
      std::unique_ptr<score::QGraphicsDraggablePixmap> button{
          Process::makePresetButton(a, ctx, &owner, nullptr)};
      auto mime = std::make_unique<QMimeData>();
      button->createDrag(*mime);
      return mime;
    };
    const auto mime = drag();
    auto presenterOf = [&](score::Document& d) {
      auto docp
          = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(d);
      REQUIRE(docp);
      Scenario::ScenarioPresenter* res{};
      for(auto p : docp->findChildren<Scenario::ScenarioPresenter*>())
        if(&p->model() == &scenarioOf(d))
          res = p;
      REQUIRE(res);
      return res;
    };

    // Dropped in the scenario: a new state holding both processes' values
    {
      const auto processes = base.processes.size();
      const auto states = scenar.states.size();
      const int before = doc->commandStack().currentIndex();
      auto& pres = *presenterOf(*doc);
      REQUIRE(app.interfaces<Scenario::DropHandlerList>().drop(
          pres, pres.fromScenarioPoint({TimeVal::fromMsecs(3000), 0.5}), *mime));
      settle();
      REQUIRE(doc->commandStack().currentIndex() == before + 1);
      REQUIRE(base.processes.size() == processes);
      REQUIRE(scenar.states.size() > states);

      const Scenario::StateModel* cue{};
      for(auto& st : scenar.states)
        if(!messages(st).empty())
          cue = &st;
      REQUIRE(cue);
      bool sawA = false, sawB = false;
      for(auto& m : messages(*cue))
      {
        if(m.address.address == LocalTree::scriptableAddress(*control(a, "level")))
        {
          REQUIRE(m.value == ossia::value{7.f});
          sawA = true;
        }
        if(m.address.address == LocalTree::scriptableAddress(*control(b, "level")))
          sawB = true;
      }
      REQUIRE(sawA);
      REQUIRE(sawB);
    }

    // Dropped in an interval (e.g. its nodal view): copies, originals kept
    {
      auto create = new Scenario::Command::CreateInterval_State_Event_TimeSync{
          scenar, scenar.states.begin()->id(), TimeVal::fromMsecs(2000), 0.3, false};
      const auto itvId = create->createdInterval();
      CommandDispatcher<>{ctx.commandStack}.submit(create);
      auto& itv = scenar.interval(itvId);
      const int before = doc->commandStack().currentIndex();
      REQUIRE(app.interfaces<Scenario::IntervalDropHandlerList>().drop(
          ctx, itv, QPointF{200, 100}, *mime));
      settle();
      REQUIRE(doc->commandStack().currentIndex() == before + 1);
      REQUIRE(itv.processes.size() == 2);
      REQUIRE(a.parent() == &base);
      REQUIRE(b.parent() == &base);
      const Process::ProcessModel* copyOfA{};
      for(auto& p : itv.processes)
        if(control(p, "spread"))
          copyOfA = &p;
      REQUIRE(copyOfA);
      REQUIRE(control(*copyOfA, "level")->value() == ossia::value{7.f});
      REQUIRE(
          LocalTree::scriptableAddress(*control(*copyOfA, "level"))
          != LocalTree::scriptableAddress(*control(a, "level")));
    }

    // Dropped in another document's scenario: a copy in a new box
    {
      auto other = score::test::new_document(app);
      settle();
      auto& oscenar = scenarioOf(*other);
      const auto intervals = oscenar.intervals.size();
      auto& pres = *presenterOf(*other);
      REQUIRE(app.interfaces<Scenario::DropHandlerList>().drop(
          pres, pres.fromScenarioPoint({TimeVal::fromMsecs(1000), 0.5}), *mime));
      settle();
      REQUIRE(oscenar.intervals.size() == intervals + 1);
      std::size_t copies = 0;
      for(auto& i : oscenar.intervals)
        copies += i.processes.size();
      REQUIRE(copies == 2);
    }
  });
}

TEST_CASE(
    "decapsulating a scenario keeps the names and the references of its processes",
    "[integration][scriptable][paste]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    CommandDispatcher<> disp{ctx.commandStack};
    auto& scenar = scenarioOf(*doc);
    auto create = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        scenar, scenar.states.begin()->id(), TimeVal::fromMsecs(2000), 0.3, false};
    const auto itvId = create->createdInterval();
    disp.submit(create);
    auto& itv = scenar.interval(itvId);

    auto add = new Scenario::Command::AddOnlyProcessToInterval{
        itv, Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), {}, QPointF{}};
    disp.submit(add);
    auto& inner = static_cast<Scenario::ProcessModel&>(itv.processes.at(add->processId()));
    auto innerCreate = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        inner, inner.states.begin()->id(), TimeVal::fromMsecs(1000), 0.3, false};
    const auto innerItvId = innerCreate->createdInterval();
    disp.submit(innerCreate);
    auto& gen = addScript(ctx, inner.interval(innerItvId), QStringLiteral("gen"), scriptA);
    settle();

    const auto genAddr = LocalTree::scriptableAddress(*control(gen, "level"));
    REQUIRE(genAddr.isSet());
    auto& end = scenar.state(itv.endState());
    disp.submit<Scenario::Command::AddMessagesToState>(
        end, State::MessageList{{State::AddressAccessor{genAddr}, 2.f}});
    REQUIRE(anchoredTo(messages(end).at(0), ctx) == control(gen, "level"));

    Scenario::DecapsulateScenario(inner, ctx.commandStack);
    settle();

    JS::ProcessModel* copy{};
    for(auto& i : scenar.intervals)
      for(auto& p : i.processes)
        if(auto js = qobject_cast<JS::ProcessModel*>(&p))
          copy = js;
    REQUIRE(copy);
    REQUIRE(LocalTree::scriptableAddress(*control(*copy, "level")) == genAddr);
    const auto m = messages(end).at(0);
    REQUIRE(m.address.address == genAddr);
    REQUIRE(anchoredTo(m, ctx) == control(*copy, "level"));
  });
}

TEST_CASE(
    "the states referring to a removed process are told, and again on undo",
    "[integration][scriptable][references]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    CommandDispatcher<> disp{ctx.commandStack};
    auto& base = score::test::base_interval(*doc);
    auto& gen = addScript(ctx, base, QStringLiteral("gen"), scriptA);
    settle();
    auto& state = some_state(*doc);
    const auto genId = gen.id();
    disp.submit<Scenario::Command::AddMessagesToState>(
        state, State::MessageList{
                   {State::AddressAccessor{LocalTree::scriptableAddress(*control(gen, "level"))},
                    2.f}});
    settle();

    auto& tree = ctx.plugin<LocalTree::DocumentPlugin>();
    bool told = false;
    auto telling = QObject::connect(
        &tree, &LocalTree::ScriptableTreeBase::referencesChanged, &tree,
        [&](QObject* referrer) { told = told || !referrer || referrer == &state; });

    disp.submit(new Scenario::Command::RemoveProcessFromInterval{base, genId});
    settle();
    REQUIRE(told);
    REQUIRE(!tree.references().broken().empty());

    told = false;
    doc->commandStack().undo();
    settle();
    REQUIRE(told);
    REQUIRE(tree.references().broken().empty());
    QObject::disconnect(telling);
  });
}

TEST_CASE(
    "a document holding anchored addresses is saved in format 5, and format 4 still loads",
    "[integration][scriptable][references]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.path() + "/anchored.score";
    {
      auto doc = score::test::new_document(app);
      const auto& ctx = doc->context();
      auto& gen = addScript(ctx, score::test::base_interval(*doc), QStringLiteral("gen"), scriptA);
      settle();
      CommandDispatcher<>{ctx.commandStack}.submit<Scenario::Command::AddMessagesToState>(
          some_state(*doc),
          State::MessageList{
              {State::AddressAccessor{LocalTree::scriptableAddress(*control(gen, "level"))},
               2.f}});
      REQUIRE(anchoredTo(messages(some_state(*doc)).at(0), ctx));
      REQUIRE(app.docManager.saveDocumentAs(*doc, path));
      app.docManager.forceCloseDocument(app, *doc);
    }

    auto json = [&] {
      QFile f{path};
      REQUIRE(f.open(QIODevice::ReadOnly));
      return f.readAll();
    };
    REQUIRE(readJson(json())["Version"].GetInt() == 5);

    auto load = [&] {
      auto doc = app.docManager.loadFile(app, path);
      REQUIRE(doc);
      settle();
      const auto& ctx = doc->context();
      const auto m = messages(some_state(*doc)).at(0);
      auto port = anchoredTo(m, ctx);
      REQUIRE(port);
      REQUIRE(port->name() == QStringLiteral("level"));
      app.docManager.forceCloseDocument(app, *doc);
    };
    load();

    // The same document stamped with the previous format: the document's
    // version is the last "Version" key, written after the plug-ins
    auto data = json();
    const auto key = data.lastIndexOf("\"Version\"");
    REQUIRE(key >= 0);
    const auto five = data.indexOf('5', key);
    REQUIRE(five > key);
    data[five] = '4';
    REQUIRE(readJson(data)["Version"].GetInt() == 4);
    {
      QFile f{path};
      REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
      f.write(data);
    }
    load();
  });
}

TEST_CASE(
    "the preset button drops the controls, and the state or a copy when chosen",
    "[integration][scriptable][snapshot]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& base = score::test::base_interval(*doc);
    auto& scenar = scenarioOf(*doc);
    auto& a = addScript(ctx, base, QStringLiteral("alpha"), scriptA);
    auto add = new Scenario::Command::AddOnlyProcessToInterval{
        base, Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), {}, QPointF{}};
    CommandDispatcher<>{ctx.commandStack}.submit(add);
    auto& inner = base.processes.at(add->processId());
    settle();

    // A script has controls and a state; a scenario's state is not offered
    {
      const auto js = Scenario::presetDropChoices({&a}, true);
      REQUIRE(js.controls);
      REQUIRE(js.controlsAndState);
      REQUIRE(js.copy);
      const auto sc = Scenario::presetDropChoices({&inner}, true);
      REQUIRE(!sc.controls);
      REQUIRE(!sc.controlsAndState);
    }

    QObject owner;
    std::unique_ptr<score::QGraphicsDraggablePixmap> button{
        Process::makePresetButton(a, ctx, &owner, nullptr)};
    QMimeData mime;
    button->createDrag(mime);

    auto docp
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
    REQUIRE(docp);
    Scenario::ScenarioPresenter* pres{};
    for(auto p : docp->findChildren<Scenario::ScenarioPresenter*>())
      if(&p->model() == &scenar)
        pres = p;
    REQUIRE(pres);

    struct Chooser
    {
      ~Chooser() { Scenario::presetDropChooser = {}; }
    } resetChooser;
    auto dropWith = [&](std::optional<Scenario::PresetDrop> choice) {
      if(choice)
        Scenario::presetDropChooser
            = [choice](const Scenario::PresetDropChoices&) { return choice; };
      else
        Scenario::presetDropChooser = {};
      REQUIRE(app.interfaces<Scenario::DropHandlerList>().drop(
          *pres, pres->fromScenarioPoint({TimeVal::fromMsecs(3000), 0.5}), mime));
      settle();
    };
    auto cue = [&]() -> const Scenario::StateModel* {
      for(auto& st : scenar.states)
        if(!messages(st).empty())
          return &st;
      return nullptr;
    };
    const auto stateAddr = LocalTree::scriptableStateAddress(a);
    auto holdsState = [&](const Scenario::StateModel& st) {
      return ossia::any_of(
          messages(st), [&](auto& m) { return m.address.address == stateAddr; });
    };
    const int before = doc->commandStack().currentIndex();

    // Plain drop: the controls only
    dropWith(std::nullopt);
    REQUIRE(cue());
    REQUIRE(!holdsState(*cue()));
    REQUIRE(!messages(*cue()).empty());
    doc->commandStack().undo();
    REQUIRE(!cue());

    // Chosen: with the state
    dropWith(Scenario::PresetDrop::ControlsAndState);
    REQUIRE(cue());
    REQUIRE(stateAddr.isSet());
    REQUIRE(holdsState(*cue()));
    doc->commandStack().undo();

    // Chosen: a copy in a new box
    const auto intervals = scenar.intervals.size();
    dropWith(Scenario::PresetDrop::Copy);
    REQUIRE(!cue());
    REQUIRE(scenar.intervals.size() == intervals + 1);
    doc->commandStack().undo();

    // Dismissed: nothing
    Scenario::presetDropChooser
        = [](const Scenario::PresetDropChoices&) -> std::optional<Scenario::PresetDrop> {
      return std::nullopt;
    };
    REQUIRE(app.interfaces<Scenario::DropHandlerList>().drop(
        *pres, pres->fromScenarioPoint({TimeVal::fromMsecs(3000), 0.5}), mime));
    settle();
    REQUIRE(doc->commandStack().currentIndex() == before);
  });
}

namespace
{
// A scenario "scene" in its own interval, holding "lfo" and a state that
// refers to lfo's level
struct Scene
{
  Scenario::IntervalModel* outer{};
  Scenario::ProcessModel* scene{};
  JS::ProcessModel* lfo{};
  Scenario::StateModel* cue{};
};

Scene makeScene(score::Document& doc)
{
  const auto& ctx = doc.context();
  CommandDispatcher<> disp{ctx.commandStack};
  auto& scenar = scenarioOf(doc);
  Scene s;
  auto create = new Scenario::Command::CreateInterval_State_Event_TimeSync{
      scenar, scenar.states.begin()->id(), TimeVal::fromMsecs(3000), 0.3, false};
  const auto outerId = create->createdInterval();
  disp.submit(create);
  s.outer = &scenar.interval(outerId);
  auto add = new Scenario::Command::AddOnlyProcessToInterval{
      *s.outer, Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), {}, QPointF{}};
  disp.submit(add);
  s.scene = static_cast<Scenario::ProcessModel*>(&s.outer->processes.at(add->processId()));
  disp.submit<Process::RenameProcess>(*s.scene, QStringLiteral("scene"));
  auto inner = new Scenario::Command::CreateInterval_State_Event_TimeSync{
      *s.scene, s.scene->states.begin()->id(), TimeVal::fromMsecs(1000), 0.3, false};
  const auto innerId = inner->createdInterval();
  disp.submit(inner);
  auto& innerItv = s.scene->interval(innerId);
  s.lfo = &addScript(ctx, innerItv, QStringLiteral("lfo"), scriptA);
  settle();
  s.cue = &s.scene->state(innerItv.endState());
  disp.submit<Scenario::Command::AddMessagesToState>(
      *s.cue, State::MessageList{
                  {State::AddressAccessor{LocalTree::scriptableAddress(*control(*s.lfo, "level"))},
                   4.f}});
  settle();
  REQUIRE(anchoredTo(messages(*s.cue).at(0), ctx) == control(*s.lfo, "level"));
  return s;
}

// The copy of the scene in the interval, and the lfo and cue in it
std::tuple<Scenario::ProcessModel*, JS::ProcessModel*, const Scenario::StateModel*>
copyOfScene(Scenario::IntervalModel& itv, const Scenario::ProcessModel* except)
{
  for(auto& p : itv.processes)
    if(auto sc = qobject_cast<Scenario::ProcessModel*>(&p); sc && sc != except)
    {
      JS::ProcessModel* lfo{};
      for(auto js : sc->findChildren<JS::ProcessModel*>())
        lfo = js;
      const Scenario::StateModel* cue{};
      for(auto& st : sc->states)
        if(!messages(st).empty())
          cue = &st;
      return {sc, lfo, cue};
    }
  return {};
}

void requireOwnCopy(
    Scenario::IntervalModel& itv, const Scene& original, const score::DocumentContext& ctx)
{
  auto [copy, lfo, cue] = copyOfScene(itv, original.scene);
  REQUIRE(copy);
  REQUIRE(lfo);
  REQUIRE(lfo != original.lfo);
  REQUIRE(cue);
  const auto m = messages(*cue).at(0);
  CHECK(anchoredTo(m, ctx) == control(*lfo, "level"));
  CHECK(m.address.address == LocalTree::scriptableAddress(*control(*lfo, "level")));
}
}

TEST_CASE(
    "find and replace in a state anchors the message to its new address",
    "[integration][scriptable][references]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& gen = addScript(ctx, score::test::base_interval(*doc), QStringLiteral("gen"), scriptA);
    settle();
    auto& state = some_state(*doc);
    const auto level = LocalTree::scriptableAddress(*control(gen, "level"));
    const auto spread = LocalTree::scriptableAddress(*control(gen, "spread"));
    CommandDispatcher<>{ctx.commandStack}.submit<Scenario::Command::AddMessagesToState>(
        state, State::MessageList{{State::AddressAccessor{level}, 0.3f}});
    REQUIRE(anchoredTo(messages(state).at(0), ctx) == control(gen, "level"));

    {
      Scenario::Command::Macro m{new Scenario::Command::RefreshStatesMacro, ctx};
      QObject* sel[] = {&state};
      m.findAndReplace(sel, level, spread);
      m.commit();
    }
    CHECK(messages(state).at(0).address.address == spread);
    CHECK(anchoredTo(messages(state).at(0), ctx) == control(gen, "spread"));

    doc->commandStack().undo();
    CHECK(messages(state).at(0).address.address == level);
    CHECK(anchoredTo(messages(state).at(0), ctx) == control(gen, "level"));
  });
}

TEST_CASE(
    "a scenario process dragged from another document drives its own copies",
    "[integration][scriptable][paste]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto a = score::test::new_document(app);
    auto scene = makeScene(*a);
    JSONReader r;
    r.stream.StartObject();
    Process::copyProcess(r, *scene.scene);
    r.obj["Path"] = score::IDocument::path(*scene.scene);
    r.stream.EndObject();
    QMimeData mime;
    mime.setData(score::mime::layerdata(), r.toByteArray());

    auto b = score::test::new_document(app);
    auto other = makeScene(*b);
    const auto& ctx = b->context();
    REQUIRE(app.interfaces<Scenario::IntervalDropHandlerList>().drop(
        ctx, *other.outer, QPointF{10, 10}, mime));
    settle();
    requireOwnCopy(*other.outer, other, ctx);
  });
}

TEST_CASE(
    "a scenario process duplicated from the object tree drives its own copies",
    "[integration][scriptable][paste]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto scene = makeScene(*doc);
    const auto& ctx = doc->context();
    Scenario::duplicateProcess(*scene.outer, *scene.scene, ctx);
    settle();
    requireOwnCopy(*scene.outer, scene, ctx);
    // The original is unchanged
    CHECK(anchoredTo(messages(*scene.cue).at(0), ctx) == control(*scene.lfo, "level"));
  });
}

TEST_CASE(
    "a score file dropped in a scenario drives its own copies",
    "[integration][scriptable][paste]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.path() + "/dropped.score";
    {
      auto a = score::test::new_document(app);
      const auto& ctx = a->context();
      auto& gen = addScript(ctx, score::test::base_interval(*a), QStringLiteral("gen"), scriptA);
      settle();
      auto& end = score::IDocument::get<Scenario::ScenarioDocumentModel>(*a)
                      .baseScenario()
                      .endState();
      CommandDispatcher<>{ctx.commandStack}.submit<Scenario::Command::AddMessagesToState>(
          end, State::MessageList{
                   {State::AddressAccessor{LocalTree::scriptableAddress(*control(gen, "level"))},
                    2.f}});
      REQUIRE(app.docManager.saveDocumentAs(*a, path));
      app.docManager.forceCloseDocument(app, *a);
    }

    auto b = score::test::new_document(app);
    const auto& ctx = b->context();
    auto& own = addScript(ctx, score::test::base_interval(*b), QStringLiteral("gen"), scriptA);
    settle();
    auto& scenar = scenarioOf(*b);
    auto docp
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*b);
    REQUIRE(docp);
    Scenario::ScenarioPresenter* pres{};
    for(auto p : docp->findChildren<Scenario::ScenarioPresenter*>())
      if(&p->model() == &scenar)
        pres = p;
    REQUIRE(pres);
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(path)});
    REQUIRE(app.interfaces<Scenario::DropHandlerList>().drop(
        *pres, pres->fromScenarioPoint({TimeVal::fromMsecs(1000), 0.5}), mime));
    settle();

    // The pasted copy of gen, and the pasted state holding a message
    JS::ProcessModel* copy{};
    for(auto& i : scenar.intervals)
      for(auto& p : i.processes)
        if(auto js = qobject_cast<JS::ProcessModel*>(&p); js && js != &own)
          copy = js;
    REQUIRE(copy);
    const Scenario::StateModel* cue{};
    for(auto& st : scenar.states)
      if(!messages(st).empty())
        cue = &st;
    REQUIRE(cue);
    const auto m = messages(*cue).at(0);
    CHECK(anchoredTo(m, ctx) == control(*copy, "level"));
    CHECK(m.address.address == LocalTree::scriptableAddress(*control(*copy, "level")));
  });
}

TEST_CASE(
    "a scriptable scenario has no state to recall",
    "[integration][scriptable][snapshot]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto scene = makeScene(*doc);
    CommandDispatcher<>{doc->context().commandStack}.submit<Process::SetProcessScriptable>(
        *scene.scene, true);
    settle();
    CHECK(Process::stateBeyondControls(*scene.scene).isEmpty());
    CHECK(!LocalTree::scriptableStateAddress(*scene.scene).isSet());
    CHECK(!Scenario::presetDropChoices({scene.scene}, true).controlsAndState);
  });
}

TEST_CASE(
    "the control messages of a state override the controls its program state sets",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& js = addScript(ctx, score::test::base_interval(*doc), QStringLiteral("gen"), scriptA);
    settle();
    auto& state = some_state(*doc);
    Scenario::Command::snapshotProcessInState(state, js, ctx);
    settle();
    // The cue's own value for level, then another program
    CommandDispatcher<>{ctx.commandStack}.submit<Scenario::Command::AddMessagesToState>(
        state, State::MessageList{
                   {State::AddressAccessor{LocalTree::scriptableAddress(*control(js, "level"))},
                    5.f}});
    CommandDispatcher<>{ctx.commandStack}.submit(
        new JS::EditScript{js, JS::QmlSource{scriptB, {}}, ctx});
    settle();
    REQUIRE(!control(js, "spread"));

    {
      ossia::execution_state st;
      st.register_device(&ctx.plugin<LocalTree::DocumentPlugin>().device());
      st.apply_device_changes();
      auto s = Engine::score_to_ossia::state(state, st);
      std::thread{[&] { s.launch(); }}.join();
    }
    settle();
    REQUIRE(control(js, "spread"));
    CHECK(control(js, "level")->value() == ossia::value{5.f});
  });
}

TEST_CASE(
    "publishing a process does not change its name",
    "[integration][scriptable][names]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& js = addScript(ctx, score::test::base_interval(*doc), QStringLiteral("gen"), scriptA);
    CommandDispatcher<>{ctx.commandStack}.submit<Process::SetProcessScriptable>(js, false);
    settle();
    const auto name = js.metadata().getName();
    CommandDispatcher<>{ctx.commandStack}.submit<Process::SetProcessScriptable>(js, true);
    settle();
    CHECK(js.metadata().getName() == name);
  });
}

TEST_CASE(
    "a plain snapshot of a process publishes it once rather than each control",
    "[integration][scriptable][snapshot]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    CommandDispatcher<> disp{ctx.commandStack};
    auto& itv = score::test::base_interval(*doc);
    auto cmd = new Scenario::Command::AddOnlyProcessToInterval{
        itv, UuidKey<Process::ProcessModel>::fromString(js_uuid), {}, QPointF{}};
    disp.submit(cmd);
    auto& js = static_cast<JS::ProcessModel&>(itv.processes.at(cmd->processId()));
    disp.submit(new JS::EditScript{js, JS::QmlSource{scriptA, {}}, ctx});
    settle();
    REQUIRE(!js.scriptable());

    auto& state = some_state(*doc);
    Scenario::Command::snapshotProcessesInState(state, {&js}, ctx, false);
    settle();
    CHECK(js.scriptable());
    CHECK(!control(js, "level")->scriptable());
    CHECK(!control(js, "spread")->scriptable());
    CHECK(messages(state).size() == 2);
  });
}

TEST_CASE(
    "malformed values written to the state of a process change nothing",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& js = addScript(ctx, score::test::base_interval(*doc), QStringLiteral("gen"), scriptA);
    settle();
    const auto addr = LocalTree::scriptableStateAddress(js);
    REQUIRE(addr.isSet());
    auto n = node(ctx, addr);
    REQUIRE(n);
    auto param = n->get_parameter();
    REQUIRE(param);
    const auto before = Process::stateBeyondControls(js);

    // Only a state holding data for the process is applied, as a preset load
    // that the process may ignore
    const std::pair<const char*, int> inputs[]
        = {{"", 0},           {"{", 0},           {"[]", 0},
           {"3", 0},          {"{}", 0},          {R"({"Data":1})", 1},
           {R"({"Script":3})", 0}, {R"({"Data":{"Script":[1,2]}})", 1},
           {R"({"Text":5})", 0},   {R"({"Controls":"x"})", 0}};
    for(auto [text, edits] : inputs)
    {
      INFO(text);
      const int c0 = doc->commandStack().currentIndex();
      std::thread{[&, text = text] { param->push_value(std::string{text}); }}.join();
      settle();
      CHECK(doc->commandStack().currentIndex() == c0 + edits);
    }
    CHECK(Process::stateBeyondControls(js) == before);
    CHECK(control(js, "level"));
  });
}

TEST_CASE(
    "a cue recalls the controls of a process laid out in time, not its content",
    "[integration][scriptable][snapshot]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& itv = score::test::base_interval(*doc);
    CommandDispatcher<> disp{ctx.commandStack};
    auto add = [&](const UuidKey<Process::ProcessModel>& key) -> Process::ProcessModel& {
      auto cmd = new Scenario::Command::AddOnlyProcessToInterval{itv, key, {}, QPointF{}};
      disp.submit(cmd);
      return itv.processes.at(cmd->processId());
    };
    auto& automation = add(Metadata<ConcreteKey_k, Automation::ProcessModel>::get());
    auto& nodal = add(Metadata<ConcreteKey_k, Nodal::Model>::get());
    auto& js = addScript(ctx, itv, QStringLiteral("gen"), scriptA);
    settle();

    for(Process::ProcessModel* p : {&automation, &nodal})
    {
      INFO(p->prettyName().toStdString());
      CHECK(!Process::recallsState(*p));
      CHECK(Process::stateBeyondControls(*p).isEmpty());
      CHECK(!Scenario::presetDropChoices({p}, true).controlsAndState);
      disp.submit<Process::SetProcessScriptable>(*p, true);
      settle();
      CHECK(!LocalTree::scriptableStateAddress(*p).isSet());
    }
    // A process that is not laid out in time keeps its state
    CHECK(Process::recallsState(js));
    CHECK(Scenario::presetDropChoices({&js}, true).controlsAndState);
  });
}

TEST_CASE(
    "the state of a process is not applied to another kind of process",
    "[integration][scriptable][recall]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    auto doc = score::test::new_document(app);
    const auto& ctx = doc->context();
    auto& js = addScript(ctx, score::test::base_interval(*doc), QStringLiteral("gen"), scriptA);
    auto& shader = addShader(*doc, writeShader(dir, "a.fs", shaderA));
    settle();
    const auto jsState = Process::stateBeyondControls(js);
    REQUIRE(!jsState.isEmpty());
    const auto before = Process::stateBeyondControls(shader);
    const int commands = doc->commandStack().currentIndex();

    // As a cue of "gen" reaching a shader that took its name
    Process::applyStateBeyondControls(shader, jsState);
    settle();
    CHECK(Process::stateBeyondControls(shader) == before);
    CHECK(doc->commandStack().currentIndex() == commands);
  });
}
