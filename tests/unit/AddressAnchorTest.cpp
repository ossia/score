// Address anchors survive copies and serialization, resolve by object
// identity rather than by name, and are kept in a state's message tree.

#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/MessageListSerialization.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/State/MessageNode.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/Properties.hpp>

#include <Scenario/Commands/Event/SetCondition.hpp>
#include <Scenario/Commands/TimeSync/SetTrigger.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Commands/State/RebindReference.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <LocalTree/ScriptableReference.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/path/Path.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <catch2/catch_all.hpp>

namespace
{
const QString smooth_uuid = QStringLiteral("bf603921-5a48-4aa5-9bc1-48a762be6467");

Process::ControlInlet& control(Process::ProcessModel& p, const QString& name)
{
  for(auto inlet : p.inlets())
    if(auto ctl = qobject_cast<Process::ControlInlet*>(inlet); ctl && ctl->name() == name)
      return *ctl;
  FAIL("no control named " << name.toStdString());
  throw;
}

Scenario::StateModel& some_state(score::Document& doc)
{
  auto& itv = score::test::base_interval(doc);
  for(auto& proc : itv.processes)
    if(auto* s = qobject_cast<Scenario::ProcessModel*>(&proc))
      return *s->states.begin();
  FAIL("no scenario in the base interval");
  throw;
}

State::AddressAccessor anchored(const QString& str, const QObject& target)
{
  auto acc = *State::parseAddressAccessor(str);
  acc.address.anchor = std::make_shared<const State::Anchor>(
      State::Anchor{score::IDocument::unsafe_path(target), {}});
  return acc;
}

template <typename T>
T json_roundtrip(const T& t)
{
  const auto doc = readJson(JSONReader::marshall(t).toByteArray());
  return JSONWriter::unmarshall<T>(doc);
}

template <typename T>
T bytes_roundtrip(const T& t)
{
  return DataStreamWriter::unmarshall<T>(DataStreamReader::marshall(t));
}
}

TEST_CASE("an anchor is shared by the copies of an address and kept by both serializations")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));

    const auto acc = anchored("score:/controls/Smooth/amount", ctl);
    REQUIRE(acc.address.anchored());

    auto copy = acc;
    REQUIRE(copy.address.anchor == acc.address.anchor);
    REQUIRE(copy == acc);

    auto plain = *State::parseAddressAccessor("score:/controls/Smooth/amount");
    REQUIRE(plain == acc);
    REQUIRE(!plain.address.anchored());

    for(auto& back : {json_roundtrip(acc), bytes_roundtrip(acc)})
    {
      REQUIRE(back == acc);
      REQUIRE(back.address.anchored());
      REQUIRE(back.address.anchor->target == acc.address.anchor->target);
      REQUIRE(back.address.anchor->resolve(doc->context()) == &ctl);
    }
    REQUIRE(!json_roundtrip(plain).address.anchored());

    State::Message m{acc, 0.5f};
    auto m2 = json_roundtrip(m);
    REQUIRE(m2.address.address.anchored());
    REQUIRE(m2.value == m.value);
  });
}

TEST_CASE("an anchor resolves by identity, not by name, and goes null with its object")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    const auto acc = anchored("score:/controls/Smooth/amount", ctl);

    proc->metadata().setName(QStringLiteral("something else"));
    ctl.setName(QStringLiteral("renamed"));
    REQUIRE(acc.address.anchor->resolve(doc->context()) == &ctl);

    // Undoing the process creation unresolves the anchor, redoing resolves it again
    doc->commandStack().undo();
    REQUIRE(acc.address.anchor->resolve(doc->context()) == nullptr);
    doc->commandStack().redo();
    auto resolved = acc.address.anchor->resolve(doc->context());
    REQUIRE(resolved);
    REQUIRE(qobject_cast<Process::ControlInlet*>(resolved));
  });
}

TEST_CASE("a message keeps its anchor through the message tree of a state")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    auto& state = some_state(*doc);

    State::MessageList list{
        State::Message{anchored("score:/controls/Smooth/amount", ctl), 0.5f},
        State::Message{*State::parseAddressAccessor("osc:/plain"), 1}};
    CommandDispatcher<>{doc->context().commandStack}
        .submit<Scenario::Command::AddMessagesToState>(state, list);

    auto msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs.size() == 2);
    for(auto& m : msgs)
    {
      if(m.address.address.device == "score")
      {
        REQUIRE(m.address.address.anchored());
        REQUIRE(m.address.address.anchor->resolve(doc->context()) == &ctl);
      }
      else
      {
        REQUIRE(!m.address.address.anchored());
      }
    }

    auto reloaded = score::test::reload_via_json(ctx, *doc);
    REQUIRE(reloaded);
    auto msgs2 = Process::flatten(some_state(*reloaded).messages().rootNode());
    REQUIRE(msgs2.size() == 2);
    int anchored_count = 0;
    for(auto& m : msgs2)
      if(m.address.address.anchored())
      {
        anchored_count++;
        REQUIRE(qobject_cast<Process::ControlInlet*>(
            m.address.address.anchor->resolve(reloaded->context())));
      }
    REQUIRE(anchored_count == 1);
  });
}

TEST_CASE("an address typed by name is anchored by the commands that store it")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    disp.submit<Process::SetPortScriptable>(ctl, true);
    disp.submit<Process::SetPortScriptingName>(ctl, QStringLiteral("wet"));
    QApplication::processEvents();

    auto& state = some_state(*doc);
    State::MessageList list{
        State::Message{*State::parseAddressAccessor("score:/controls/fx/wet"), 0.5f}};
    disp.submit<Scenario::Command::AddMessagesToState>(state, list);
    auto msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0].address.address.anchored());
    REQUIRE(msgs[0].address.address.anchor->resolve(doc->context()) == &ctl);

    auto proc2 = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc2);
    auto& other = control(*proc2, QStringLiteral("Amount"));
    disp.submit<Process::ChangePortAddress>(
        other, *State::parseAddressAccessor("score:/controls/fx/wet"));
    REQUIRE(other.address().address.anchored());
    REQUIRE(other.address().address.anchor->resolve(doc->context()) == &ctl);

    auto& event = *some_state(*doc).parent()->findChild<Scenario::EventModel*>();
    disp.submit<Scenario::Command::SetCondition>(
        event, *State::parseExpression(QStringLiteral("%score:/controls/fx/wet% > 0.2")));
    const State::Relation* rel{};
    auto find_relation = [&](auto& self, const State::Expression& e) -> void {
      if(auto r = e.target<State::Relation>())
        rel = r;
      for(auto& child : e)
        self(self, child);
    };
    find_relation(find_relation, event.condition());
    REQUIRE(rel);
    if(auto lhs = rel->lhs.target<State::AddressAccessor>())
      REQUIRE(lhs->address.anchored());
    else if(auto lhs = rel->lhs.target<State::Address>())
      REQUIRE(lhs->anchored());
    else
      FAIL("the left operand is not an address");

    // The anchor survives a rename; the address follows the new name
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("reverb"));
    const auto now = LocalTree::derived(msgs[0].address, doc->context());
    REQUIRE(now.address.path == QStringList{"controls", "reverb", "wet"});
    REQUIRE(now.address.anchor == msgs[0].address.address.anchor);
  });
}

TEST_CASE("a document written with names alone is anchored once loaded")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    disp.submit<Process::SetPortScriptable>(ctl, true);
    disp.submit<Process::SetPortScriptingName>(ctl, QStringLiteral("wet"));

    // A message without an anchor
    auto& state = some_state(*doc);
    disp.submit<Scenario::Command::AddMessagesToState>(
        state,
        State::MessageList{
            State::Message{*State::parseAddressAccessor("score:/controls/fx/wet"), 0.5f}});
    Process::MessageNode tree = state.messages().rootNode();
    auto strip = [](auto& self, Process::MessageNode& n) -> void {
      n.anchor.reset();
      for(auto& child : n)
        self(self, child);
    };
    strip(strip, tree);
    state.messages() = std::move(tree);
    REQUIRE(!Process::flatten(state.messages().rootNode())[0].address.address.anchored());

    auto reloaded = score::test::reload_via_json(ctx, *doc);
    REQUIRE(reloaded);
    QApplication::processEvents();
    QApplication::processEvents();
    auto msgs = Process::flatten(some_state(*reloaded).messages().rootNode());
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0].address.address.anchored());
    auto target = msgs[0].address.address.anchor->resolve(reloaded->context());
    REQUIRE(target);
    REQUIRE(qobject_cast<Process::ControlInlet*>(target)->exposed() == "wet");
  });
}

TEST_CASE("renaming a published object rewrites what refers to it, and undo brings it back")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    disp.submit<Process::SetPortScriptable>(ctl, true);
    disp.submit<Process::SetPortScriptingName>(ctl, QStringLiteral("wet"));
    QApplication::processEvents();

    auto& state = some_state(*doc);
    disp.submit<Scenario::Command::AddMessagesToState>(
        state,
        State::MessageList{
            State::Message{*State::parseAddressAccessor("score:/controls/fx/wet"), 0.5f}});
    auto proc2 = score::test::add_process(*doc, smooth_uuid, {});
    auto& other = control(*proc2, QStringLiteral("Amount"));
    disp.submit<Process::ChangePortAddress>(
        other, *State::parseAddressAccessor("score:/controls/fx/wet"));
    auto& event = *some_state(*doc).parent()->findChild<Scenario::EventModel*>();
    disp.submit<Scenario::Command::SetCondition>(
        event, *State::parseExpression(QStringLiteral("%score:/controls/fx/wet% > 0.2")));

    auto& index = doc->context().plugin<LocalTree::DocumentPlugin>().references();
    REQUIRE(index.referrers(ctl).size() == 3);
    REQUIRE(index.targets(state) == std::vector<QObject*>{&ctl});

    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("reverb"));
    disp.submit<Process::SetPortScriptingName>(ctl, QStringLiteral("mix"));

    auto msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0].address.address.path == QStringList{"controls", "reverb", "mix"});
    REQUIRE(msgs[0].value == ossia::value{0.5f});
    REQUIRE(other.address().address.path == QStringList{"controls", "reverb", "mix"});
    REQUIRE(event.condition().toString().contains("score:/controls/reverb/mix"));
    REQUIRE(index.referrers(ctl).size() == 3);

    doc->commandStack().undo();
    doc->commandStack().undo();
    msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs[0].address.address.path == QStringList{"controls", "fx", "wet"});
    REQUIRE(other.address().address.path == QStringList{"controls", "fx", "wet"});
    REQUIRE(event.condition().toString().contains("score:/controls/fx/wet"));
  });
}

TEST_CASE("a reference to what is not published keeps its address until something is")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    CommandDispatcher<> disp{doc->context().commandStack};
    auto& index = doc->context().plugin<LocalTree::DocumentPlugin>().references();

    // A message added before the control is published
    auto& state = some_state(*doc);
    disp.submit<Scenario::Command::AddMessagesToState>(
        state,
        State::MessageList{
            State::Message{*State::parseAddressAccessor("score:/controls/fx/wet"), 0.5f}});
    REQUIRE(!Process::flatten(state.messages().rootNode())[0].address.address.anchored());
    REQUIRE(index.broken().size() == 1);

    auto publish = [&] {
      auto proc = score::test::add_process(*doc, smooth_uuid, {});
      REQUIRE(proc);
      auto& ctl = control(*proc, QStringLiteral("Amount"));
      disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
      disp.submit<Process::SetPortScriptable>(ctl, true);
      disp.submit<Process::SetPortScriptingName>(ctl, QStringLiteral("wet"));
      return &ctl;
    };

    auto ctl = publish();
    REQUIRE(index.broken().empty());
    auto msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0].address.address.anchor->resolve(doc->context()) == ctl);
    REQUIRE(index.referrers(*ctl) == std::vector<QObject*>{&state});

    // Once the process is removed, the message keeps the last address it was bound to
    for(int i = 0; i < 4; i++)
      doc->commandStack().undo();
    msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0].address.address.path == QStringList{"controls", "fx", "amount"});
    REQUIRE(index.broken().size() == 1);

    // Publishing another control under the same name rebinds the message
    auto ctl2 = publish();
    REQUIRE(index.broken().empty());
    msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs[0].address.address.anchor->resolve(doc->context()) == ctl2);
    REQUIRE(msgs[0].address.address.path == QStringList{"controls", "fx", "wet"});
    REQUIRE(index.referrers(*ctl2) == std::vector<QObject*>{&state});
  });
}

TEST_CASE("a broken reference is reported as such and can be pointed elsewhere")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    disp.submit<Process::SetPortScriptable>(ctl, true);
    disp.submit<Process::SetPortScriptingName>(ctl, QStringLiteral("wet"));

    const auto gone = *State::parseAddress("score:/controls/fx/dry");
    const auto there = *State::parseAddress("score:/controls/fx/wet");
    REQUIRE(LocalTree::broken(gone, doc->context()));
    REQUIRE(!LocalTree::broken(there, doc->context()));
    REQUIRE(!LocalTree::published(*State::parseAddress("osc:/x"), doc->context()));

    auto& state = some_state(*doc);
    disp.submit<Scenario::Command::AddMessagesToState>(
        state, State::MessageList{State::Message{State::AddressAccessor{gone}, 0.5f}});
    auto& event = *some_state(*doc).parent()->findChild<Scenario::EventModel*>();
    disp.submit<Scenario::Command::SetCondition>(
        event, *State::parseExpression(QStringLiteral("%score:/controls/fx/dry% > 0.2")));
    auto& index = doc->context().plugin<LocalTree::DocumentPlugin>().references();
    REQUIRE(index.broken().size() == 2);

    Scenario::Command::rebindReference(state, gone, there, doc->context());
    Scenario::Command::rebindReference(event, gone, there, doc->context());

    auto msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0].address.address == there);
    REQUIRE(msgs[0].value == ossia::value{0.5f});
    REQUIRE(msgs[0].address.address.anchor->resolve(doc->context()) == &ctl);
    REQUIRE(event.condition().toString().contains("score:/controls/fx/wet"));
    REQUIRE(index.broken().empty());

    doc->commandStack().undo();
    doc->commandStack().undo();
    REQUIRE(Process::flatten(state.messages().rootNode())[0].address.address == gone);
    REQUIRE(index.broken().size() == 2);
  });
}

namespace
{
bool allAnchored(const State::Expression& e)
{
  const auto list = State::anchors(e);
  return !list.empty() && ossia::all_of(list, [](auto& a) { return bool(a); });
}

Scenario::EventModel& some_event(score::Document& doc)
{
  auto& state = some_state(doc);
  auto scenar = qobject_cast<Scenario::ProcessModel*>(state.parent());
  REQUIRE(scenar);
  return scenar->event(state.eventId());
}
}

TEST_CASE("a condition and a trigger keep their anchors through a save and load")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    disp.submit<Process::SetPortScriptable>(ctl, true);
    disp.submit<Process::SetPortScriptingName>(ctl, QStringLiteral("wet"));

    auto& event = some_event(*doc);
    auto scenar = qobject_cast<Scenario::ProcessModel*>(event.parent());
    auto& sync = Scenario::parentTimeSync(event, *scenar);
    disp.submit<Scenario::Command::SetCondition>(
        event, *State::parseExpression(QStringLiteral("%score:/controls/fx/wet% > 0.2")));
    disp.submit<Scenario::Command::SetTrigger>(
        sync, *State::parseExpression(QStringLiteral("%score:/controls/fx/wet% < 0.8")));
    REQUIRE(allAnchored(event.condition()));
    REQUIRE(allAnchored(sync.expression()));

    auto reloaded = score::test::reload_via_json(ctx, *doc);
    REQUIRE(reloaded);
    QApplication::processEvents();
    QApplication::processEvents();
    const auto& rctx = reloaded->context();
    auto& event2 = some_event(*reloaded);
    auto scenar2 = qobject_cast<Scenario::ProcessModel*>(event2.parent());
    auto& sync2 = Scenario::parentTimeSync(event2, *scenar2);
    REQUIRE(allAnchored(event2.condition()));
    REQUIRE(allAnchored(sync2.expression()));
    auto& index = rctx.plugin<LocalTree::DocumentPlugin>().references();
    REQUIRE(index.broken().empty());

    // After reload, the addresses follow a rename of their control
    Process::ProcessModel* proc2{};
    for(auto& p : score::test::base_interval(*reloaded).processes)
      if(p.metadata().getName() == "fx")
        proc2 = &p;
    REQUIRE(proc2);
    CommandDispatcher<>{rctx.commandStack}.submit<Process::RenameProcess>(
        *proc2, QStringLiteral("reverb"));
    QApplication::processEvents();
    REQUIRE(event2.condition().toString().contains("score:/controls/reverb/wet"));
    REQUIRE(sync2.expression().toString().contains("score:/controls/reverb/wet"));
    REQUIRE(index.broken().empty());
  });
}

TEST_CASE("a condition written ahead of its control is anchored once it is published")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    CommandDispatcher<> disp{doc->context().commandStack};
    auto& index = doc->context().plugin<LocalTree::DocumentPlugin>().references();
    auto& event = some_event(*doc);
    disp.submit<Scenario::Command::SetCondition>(
        event, *State::parseExpression(QStringLiteral("%score:/controls/fx/wet% > 0.2")));
    REQUIRE(!allAnchored(event.condition()));
    REQUIRE(index.broken().size() == 1);

    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    disp.submit<Process::SetPortScriptable>(ctl, true);
    disp.submit<Process::SetPortScriptingName>(ctl, QStringLiteral("wet"));
    QApplication::processEvents();
    REQUIRE(allAnchored(event.condition()));
    REQUIRE(index.broken().empty());
    REQUIRE(State::anchors(event.condition())[0]->resolve(doc->context()) == &ctl);
  });
}

TEST_CASE("a condition set again with only another anchor takes it")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& event = some_event(*doc);
    auto expr = *State::parseExpression(QStringLiteral("%score:/controls/fx/wet% > 0.2"));
    event.setCondition(expr);
    int changes = 0;
    auto counting = QObject::connect(
        &event, &Scenario::EventModel::conditionChanged, &event, [&] { changes++; });
    auto anchoredExpr = expr;
    State::setAnchors(
        anchoredExpr, {std::make_shared<const State::Anchor>(
                          State::Anchor{score::IDocument::unsafe_path(*proc), {}})});
    event.setCondition(anchoredExpr);
    REQUIRE(changes == 1);
    REQUIRE(allAnchored(event.condition()));
    event.setCondition(anchoredExpr);
    REQUIRE(changes == 1);
    QObject::disconnect(counting);
  });
}

TEST_CASE("an address carried in from elsewhere stands for what its name publishes here")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    CommandDispatcher<> disp{dctx.commandStack};
    auto fx = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(fx);
    disp.submit<Process::RenameProcess>(*fx, QStringLiteral("fx"));
    auto publish = [&](const QString& port, const QString& name) -> Process::ControlInlet& {
      auto& ctl = control(*fx, port);
      disp.submit<Process::SetPortScriptable>(ctl, true);
      disp.submit<Process::SetPortScriptingName>(ctl, name);
      return ctl;
    };
    auto& wet = publish(QStringLiteral("Amount"), QStringLiteral("wet"));
    auto& dry = publish(QStringLiteral("Beta (1e only)"), QStringLiteral("dry"));
    QApplication::processEvents();

    // fx/wet anchored to fx/dry, as pasted from another document
    const auto foreign = anchored(QStringLiteral("score:/controls/fx/wet"), dry);
    REQUIRE(foreign.address.anchor->resolve(dctx) == &dry);

    auto& event = some_event(*doc);
    auto scenar = qobject_cast<Scenario::ProcessModel*>(event.parent());
    auto& sync = Scenario::parentTimeSync(event, *scenar);
    auto withForeign = [&](const QString& text) {
      auto e = *State::parseExpression(text);
      State::setAnchors(e, {foreign.address.anchor});
      return e;
    };
    disp.submit<Scenario::Command::SetCondition>(
        event, withForeign(QStringLiteral("%score:/controls/fx/wet% > 0.2")));
    disp.submit<Scenario::Command::SetTrigger>(
        sync, withForeign(QStringLiteral("%score:/controls/fx/wet% < 0.8")));
    REQUIRE(State::anchors(event.condition())[0]->resolve(dctx) == &wet);
    REQUIRE(State::anchors(sync.expression())[0]->resolve(dctx) == &wet);

    auto proc3 = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc3);
    auto& port = control(*proc3, QStringLiteral("Amount"));
    disp.submit<Process::ChangePortAddress>(port, foreign);
    REQUIRE(port.address().address.anchor->resolve(dctx) == &wet);

    // Renaming re-anchors to the new name; undo restores the previous anchor
    auto& state = some_state(*doc);
    disp.submit<Scenario::Command::AddMessagesToState>(
        state, State::MessageList{{*State::parseAddressAccessor("score:/controls/fx/wet"), 0.5f}});
    disp.submit<Scenario::Command::RenameAddressInState>(
        state, *State::parseAddressAccessor("score:/controls/fx/wet"),
        *State::parseAddressAccessor("score:/controls/fx/dry"));
    auto msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0].address.address.anchored());
    REQUIRE(msgs[0].address.address.anchor->resolve(dctx) == &dry);
    doc->commandStack().undo();
    msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs[0].address.address.path == QStringList{"controls", "fx", "wet"});
    REQUIRE(msgs[0].address.address.anchor->resolve(dctx) == &wet);

    // Applied after another edit of the state, as in a macro, the rename keeps it
    Scenario::Command::RenameAddressInState rename{
        state, *State::parseAddressAccessor("score:/controls/fx/wet"),
        *State::parseAddressAccessor("score:/controls/fx/dry")};
    disp.submit<Scenario::Command::AddMessagesToState>(
        state, State::MessageList{{*State::parseAddressAccessor("testdev:/other"), 0.25f}});
    rename.redo(dctx);
    msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs.size() == 2);
    REQUIRE(ossia::any_of(msgs, [&](auto& m) {
      return m.address.address.path == QStringList{"controls", "fx", "dry"}
             && m.address.address.anchor->resolve(dctx) == &dry;
    }));
    rename.undo(dctx);
    msgs = Process::flatten(state.messages().rootNode());
    REQUIRE(msgs.size() == 2);
    REQUIRE(ossia::any_of(msgs, [&](auto& m) {
      return m.address.address.path == QStringList{"controls", "fx", "wet"}
             && m.address.address.anchor->resolve(dctx) == &wet;
    }));
  });
}

TEST_CASE("the addresses of a document made before the namespace are read as they are now")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    CommandDispatcher<> disp{doc->context().commandStack};
    auto fx = score::test::add_process(*doc, smooth_uuid, {});
    auto other = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(fx);
    REQUIRE(other);
    disp.submit<Process::RenameProcess>(*fx, QStringLiteral("fx"));
    disp.submit<Process::RenameProcess>(*other, QStringLiteral("other"));
    auto& amount = control(*fx, QStringLiteral("Amount"));
    disp.submit<Process::SetPortScriptable>(amount, true);

    // Legacy addresses: device named after the document, structural process paths
    const auto base = score::test::base_interval(*doc).metadata().getName();
    auto old = [&](const QString& proc) {
      return QStringLiteral("score (old):/%1/processes/%2/%3/value")
          .arg(base, proc, amount.exposed());
    };
    auto published = State::parseAddressAccessor(old("fx"));
    auto structural = State::parseAddressAccessor(old("other"));
    REQUIRE(published);
    REQUIRE(structural);
    disp.submit<Scenario::Command::AddMessagesToState>(
        some_state(*doc), State::MessageList{{*published, 0.5f}, {*structural, 0.25f}});
    auto condition = State::parseExpression(QStringLiteral("%%1% > 0.5").arg(old("fx")));
    REQUIRE(condition);
    disp.submit<Scenario::Command::SetCondition>(some_event(*doc), std::move(*condition));

    auto reloaded = score::test::reload_via_json(ctx, *doc);
    REQUIRE(reloaded);
    QApplication::processEvents();
    QApplication::processEvents();
    const auto& rctx = reloaded->context();

    Process::ProcessModel* fx2{};
    for(auto& p : score::test::base_interval(*reloaded).processes)
      if(p.metadata().getName() == "fx")
        fx2 = &p;
    REQUIRE(fx2);
    auto& amount2 = control(*fx2, QStringLiteral("Amount"));

    auto msgs = Process::flatten(some_state(*reloaded).messages().rootNode());
    REQUIRE(msgs.size() == 2);
    bool sawPublished = false, sawStructural = false;
    for(auto& m : msgs)
    {
      const auto& a = m.address.address;
      if(m.value == ossia::value{0.5f})
      {
        REQUIRE(a == LocalTree::scriptableAddress(amount2));
        REQUIRE(a.anchored());
        REQUIRE(a.anchor->resolve(rctx) == &amount2);
        sawPublished = true;
      }
      else
      {
        REQUIRE(a.device == "score");
        REQUIRE(a.path == structural->address.path);
        sawStructural = true;
      }
    }
    REQUIRE(sawPublished);
    REQUIRE(sawStructural);

    auto& event2 = some_event(*reloaded);
    REQUIRE(event2.condition().toString().contains(
        LocalTree::scriptableAddress(amount2).toString()));
    REQUIRE(State::anchors(event2.condition())[0]);
  });
}

