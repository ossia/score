// Moving messages between states and devices, for each kind of address a
// state can hold: published control, explorer node, missing node or device, pattern.

#include <State/Address.hpp>
#include <State/MessageListSerialization.hpp>

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/State/MessageNode.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Remove.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/DeviceExplorerView.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>

#include <Scenario/Commands/Cohesion/CreateCurves.hpp>
#include <Scenario/Commands/Cohesion/InterpolateStates.hpp>
#include <Scenario/Commands/Cohesion/RefreshStates.hpp>
#include <Scenario/Commands/Cohesion/SnapshotParameters.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateSequence.hpp>
#include <Scenario/Commands/Scenario/Deletions/ClearState.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Commands/State/RemoveMessageNodes.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <Automation/AutomationModel.hpp>

#include <LocalTree/ScriptableProcessComponent.hpp>
#include <LocalTree/ScriptableReference.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/selection/SelectionDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/network/domain/domain.hpp>

#include <QAction>
#include <QApplication>
#include <QItemSelectionModel>
#include <QElapsedTimer>
#include <QMimeData>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <memory>

namespace
{
const QString smooth_uuid = QStringLiteral("bf603921-5a48-4aa5-9bc1-48a762be6467");

void settle()
{
  for(int i = 0; i < 8; i++)
  {
    QCoreApplication::sendPostedEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents(QEventLoop::AllEvents, 10);
  }
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
  for(auto& proc : score::test::base_interval(doc).processes)
    if(auto* s = qobject_cast<Scenario::ProcessModel*>(&proc))
      return *s;
  FAIL("no scenario in the base interval");
  throw;
}

State::AddressAccessor acc(const QString& s)
{
  auto a = State::parseAddressAccessor(s);
  REQUIRE(a);
  return *a;
}

State::MessageList messages(const Scenario::StateModel& state)
{
  return Process::flatten(state.messages().rootNode());
}

const State::Message* find(const State::MessageList& ml, const State::Address& a)
{
  for(auto& m : ml)
    if(m.address.address == a)
      return &m;
  return nullptr;
}

QModelIndex indexOf(
    const Scenario::MessageItemModel& model, const State::Address& a, int column,
    const QModelIndex& parent = {})
{
  for(int r = 0; r < model.rowCount(parent); r++)
  {
    auto idx = model.index(r, 0, parent);
    auto& node = model.nodeFromModelIndex(idx);
    if(node.hasValue() && Process::address(node).address == a)
      return model.index(r, column, parent);
    if(auto sub = indexOf(model, a, column, idx); sub.isValid())
      return sub;
  }
  return {};
}

Device::AddressSettings parameter(const QString& name, ossia::value v, ossia::domain dom = {})
{
  Device::AddressSettings as;
  as.name = name;
  as.value = std::move(v);
  as.domain = std::move(dom);
  as.ioType = ossia::access_mode::BI;
  return as;
}

//! OSC devices only send, so the explorer tree is the whole device.
struct Fixture
{
  score::Document* doc{};
  const score::DocumentContext* dctx{};
  Explorer::DeviceDocumentPlugin* devices{};
  Process::ProcessModel* proc{};
  Process::ControlInlet* amount{};
  Process::ControlInlet* freq{};
  State::AddressAccessor published;
  State::AddressAccessor publishedFreq;

  explicit Fixture(const score::GUIApplicationContext& ctx)
  {
    doc = score::test::new_document(ctx);
    REQUIRE(doc);
    dctx = &doc->context();
    devices = &dctx->plugin<Explorer::DeviceDocumentPlugin>();

    proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    amount = &control(*proc, QStringLiteral("Amount"));
    freq = &control(*proc, QStringLiteral("Freq (1e/LP)"));
    submit(new Process::SetPortScriptable(*amount, true));
    submit(new Process::SetPortScriptable(*freq, true));
    submit(new Process::SetValue(*amount, ossia::value{0.6f}));
    settle();
    published = State::AddressAccessor{LocalTree::scriptableAddress(*amount)};
    publishedFreq = State::AddressAccessor{LocalTree::scriptableAddress(*freq)};

    Device::ProtocolFactory* osc{};
    for(auto& f : ctx.interfaces<Device::ProtocolFactoryList>())
      if(f.prettyName() == "OSC")
        osc = &f;
    REQUIRE(osc);
    auto settings = osc->defaultSettings();
    settings.name = "testdev";
    Device::Node dev{settings, nullptr};
    dev.push_back(Device::Node{
        parameter("level", 0.75f, ossia::make_domain(0.f, 2.f)), &dev});
    dev.push_back(Device::Node{parameter("other", 0.5f), &dev});
    submit(new Explorer::Command::LoadDevice{*devices, std::move(dev)});
    REQUIRE(deviceNode());
    settle();
  }

  void submit(score::Command* cmd) { CommandDispatcher<>{dctx->commandStack}.submit(cmd); }

  Device::Node* deviceNode() const
  {
    for(auto& n : devices->rootNode())
      if(n.get<Device::DeviceSettings>().name == "testdev")
        return &n;
    return nullptr;
  }

  Scenario::StateModel& startState() const
  {
    auto& scenar = base_scenario(*doc);
    REQUIRE(scenar.states.size() > 0);
    return *scenar.states.begin();
  }

  Scenario::IntervalModel& intervalAfterStart()
  {
    auto& scenar = base_scenario(*doc);
    auto cmd = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        scenar, startState().id(), TimeVal::fromMsecs(3000), 0.4, false};
    submit(cmd);
    return scenar.interval(cmd->createdInterval());
  }

  const Automation::ProcessModel*
  automationOn(const Scenario::IntervalModel& itv, const State::Address& a) const
  {
    for(auto& p : itv.processes)
      if(auto autom = qobject_cast<const Automation::ProcessModel*>(&p))
        if(autom->address().address == a)
          return autom;
    return nullptr;
  }
};

struct AutoSequence
{
  Scenario::Settings::Model& settings;
  bool before{};
  explicit AutoSequence(const score::GUIApplicationContext& ctx)
      : settings{ctx.settings<Scenario::Settings::Model>()}
      , before{settings.getAutoSequence()}
  {
    settings.setAutoSequence(true);
  }
  ~AutoSequence() { settings.setAutoSequence(before); }
};
}

TEST_CASE(
    "refreshing a state reads published controls and the explorer, keeps anchors, and "
    "keeps what nothing answers for",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& state = f.startState();
    f.submit(new Scenario::Command::AddMessagesToState{
        state, State::MessageList{
                   {f.published, 0.1f},
                   {acc("testdev:/level"), 0.1f},
                   {acc("testdev:/nothing"), 0.2f},
                   {acc("nodevice:/a"), 0.3f},
                   {acc("score:/controls/*/Amount"), 0.4f},
                   {acc("testdev:/lev*"), 0.5f}}});
    REQUIRE(find(messages(state), f.published.address)->address.address.anchored());

    Scenario::Command::RefreshStates({&state}, *f.dctx);
    auto ml = messages(state);
    REQUIRE(ml.size() == 6);
    auto published = find(ml, f.published.address);
    REQUIRE(published->value == ossia::value{0.6f});
    REQUIRE(published->address.address.anchored());
    REQUIRE(find(ml, acc("testdev:/level").address)->value == ossia::value{0.75f});
    REQUIRE(find(ml, acc("testdev:/nothing").address)->value == ossia::value{0.2f});
    REQUIRE(find(ml, acc("nodevice:/a").address)->value == ossia::value{0.3f});
    REQUIRE(find(ml, acc("score:/controls/*/Amount").address)->value == ossia::value{0.4f});
    REQUIRE(find(ml, acc("testdev:/lev*").address)->value == ossia::value{0.5f});

    f.doc->commandStack().undo();
    REQUIRE(find(messages(state), f.published.address)->value == ossia::value{0.1f});
  });
}

TEST_CASE(
    "the message tree tells found, unpublished, missing and pattern addresses apart",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& state = f.startState();
    f.submit(new Scenario::Command::AddMessagesToState{
        state, State::MessageList{
                   {f.published, 0.1f},
                   {acc("score:/controls/nothing/here"), 0.2f},
                   {acc("testdev:/level"), 0.1f},
                   {acc("testdev:/nothing"), 0.2f},
                   {acc("nodevice:/a"), 0.3f},
                   {acc("score:/controls/*/Amount"), 0.4f},
                   {acc("testdev:/lev*"), 0.5f}}});

    auto& model = state.messages();
    auto tip = [&](const QString& a) {
      auto idx = indexOf(model, acc(a).address, 0);
      REQUIRE(idx.isValid());
      return model.data(idx, Qt::ToolTipRole).toString();
    };
    using S = LocalTree::AddressStatus;
    REQUIRE(tip(f.published.address.toString()).isEmpty());
    REQUIRE(tip("score:/controls/nothing/here") == LocalTree::describe(S::Unpublished));
    REQUIRE(tip("testdev:/level").isEmpty());
    REQUIRE(tip("testdev:/nothing") == LocalTree::describe(S::NoNode));
    REQUIRE(tip("nodevice:/a") == LocalTree::describe(S::NoDevice));
    REQUIRE(tip("score:/controls/*/Amount").isEmpty());
    REQUIRE(tip("testdev:/lev*").isEmpty());

    auto& level = f.deviceNode()->childAt(0);
    f.submit(new Explorer::Command::Remove{*f.devices, Device::NodePath{level}});
    settle();
    REQUIRE(tip("testdev:/level") == LocalTree::describe(S::NoNode));
    f.submit(new Explorer::Command::Remove{*f.devices, *f.deviceNode()});
    settle();
    REQUIRE(tip("testdev:/nothing") == LocalTree::describe(S::NoDevice));
    f.doc->commandStack().undo();
    f.doc->commandStack().undo();
    settle();
    REQUIRE(tip("testdev:/level").isEmpty());
  });
}

TEST_CASE(
    "editing a value in the message tree keeps the message anchored",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& state = f.startState();
    f.submit(new Scenario::Command::AddMessagesToState{
        state, State::MessageList{{f.published, 0.1f}, {acc("testdev:/level"), 0.1f}}});

    auto& model = state.messages();
    auto idx = indexOf(model, f.published.address, 1);
    REQUIRE(idx.isValid());
    REQUIRE(model.setData(idx, QVariant::fromValue(ossia::value{0.9f}), Qt::EditRole));
    auto m = find(messages(state), f.published.address);
    REQUIRE(m->value == ossia::value{0.9f});
    REQUIRE(m->address.address.anchored());

    idx = indexOf(model, acc("testdev:/level").address, 1);
    REQUIRE(model.setData(idx, QStringLiteral("0.25"), Qt::EditRole));
    REQUIRE(find(messages(state), acc("testdev:/level").address)->value == ossia::value{0.25f});

    f.submit(new Process::RenameProcess{*f.proc, QStringLiteral("renamed")});
    settle();
    const auto now = LocalTree::scriptableAddress(*f.amount);
    REQUIRE(now.path.contains("renamed"));
    REQUIRE(find(messages(state), now));
  });
}

TEST_CASE(
    "renaming a message in the message tree takes it off the control it stood for",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& state = f.startState();
    f.submit(new Scenario::Command::AddMessagesToState{
        state, State::MessageList{{f.published, 0.1f}}});

    auto& model = state.messages();
    auto idx = indexOf(model, f.published.address, 0);
    REQUIRE(idx.isValid());
    REQUIRE(model.setData(idx, QStringLiteral("elsewhere"), Qt::EditRole));
    auto renamed = f.published.address;
    renamed.path.back() = "elsewhere";
    renamed.anchor.reset();
    REQUIRE(find(messages(state), renamed));

    // A typed address is not changed when its process is renamed
    f.submit(new Process::RenameProcess{*f.proc, QStringLiteral("renamed")});
    settle();
    auto ml = messages(state);
    REQUIRE(ml.size() == 1);
    INFO(ml[0].address.toString().toStdString());
    REQUIRE(ml[0].address.address == renamed);

    // Once renamed to another published control, it follows that control
    f.doc->commandStack().undo();
    settle();
    auto freq = f.publishedFreq.address;
    idx = indexOf(model, renamed, 0);
    REQUIRE(idx.isValid());
    REQUIRE(model.setData(idx, freq.path.back(), Qt::EditRole));
    settle();
    f.submit(new Process::RenameProcess{*f.proc, QStringLiteral("renamed")});
    settle();
    ml = messages(state);
    REQUIRE(ml.size() == 1);
    INFO(ml[0].address.toString().toStdString());
    REQUIRE(ml[0].address.address == LocalTree::scriptableAddress(*f.freq));
  });
}

TEST_CASE(
    "messages dragged out of a state and dropped on another keep what they stand for",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& from = f.startState();
    f.submit(new Scenario::Command::AddMessagesToState{
        from, State::MessageList{
                  {f.published, 0.1f},
                  {acc("testdev:/level"), 0.2f},
                  {acc("nodevice:/a"), 0.3f}}});
    auto& itv = f.intervalAfterStart();
    auto& to = Scenario::endState(itv, base_scenario(*f.doc));

    auto& model = from.messages();
    QModelIndexList rows;
    for(int r = 0; r < model.rowCount({}); r++)
      rows.push_back(model.index(r, 0, {}));
    std::unique_ptr<QMimeData> mime{model.mimeData(rows)};
    REQUIRE(mime);
    REQUIRE(to.messages().dropMimeData(mime.get(), Qt::CopyAction, -1, -1, {}) == false);

    auto ml = messages(to);
    REQUIRE(ml.size() == 3);
    REQUIRE(find(ml, f.published.address)->address.address.anchored());
    REQUIRE(find(ml, acc("testdev:/level").address)->value == ossia::value{0.2f});
    REQUIRE(find(ml, acc("nodevice:/a").address));

    f.submit(new Process::RenameProcess{*f.proc, QStringLiteral("renamed")});
    settle();
    REQUIRE(find(messages(to), LocalTree::scriptableAddress(*f.amount)));
  });
}

TEST_CASE(
    "messages carried from another document stand for what their names publish",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& state = f.startState();

    // Resolves to the Amount control under a name this document does not publish
    auto anchored = f.published.address;
    REQUIRE(LocalTree::anchor(anchored, *f.dctx));
    State::AddressAccessor foreign{acc("score:/controls/elsewhere/Amount")};
    foreign.address.anchor = anchored.anchor;

    f.submit(new Scenario::Command::AddMessagesToState{
        state, State::MessageList{{foreign, 0.1f}}});
    settle();
    f.submit(new Process::RenameProcess{*f.proc, QStringLiteral("renamed")});
    settle();
    auto ml = messages(state);
    REQUIRE(ml.size() == 1);
    INFO(ml[0].address.toString().toStdString());
    REQUIRE(ml[0].address.address == foreign.address);
  });
}

TEST_CASE(
    "a dropped message whose anchor designates nothing here stands for what its name publishes",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& state = f.startState();
    auto& tree = f.dctx->plugin<LocalTree::DocumentPlugin>();

    // As dragged from another document, where the anchored object does not exist
    State::AddressAccessor foreign = f.published;
    foreign.address.anchor = std::make_shared<const State::Anchor>(State::Anchor{
        ObjectPath{{ObjectIdentifier{QStringLiteral("Nothing"), 4242}}}, QString{}});
    REQUIRE(!foreign.address.anchor->resolve(*f.dctx));
    QMimeData mime;
    Mime<State::MessageList>::Serializer{mime}.serialize({{foreign, 0.1f}});
    state.messages().dropMimeData(&mime, Qt::CopyAction, -1, -1, {});
    settle();

    auto ml = messages(state);
    REQUIRE(ml.size() == 1);
    REQUIRE(ml[0].address.address.anchored());
    REQUIRE(ml[0].address.address.anchor->resolve(*f.dctx) == f.amount);
    REQUIRE(tree.references().broken().empty());

    f.submit(new Process::RenameProcess{*f.proc, QStringLiteral("renamed")});
    settle();
    REQUIRE(find(messages(state), LocalTree::scriptableAddress(*f.amount)));

    // Without a published name, the anchor stays for a later recovery
    State::AddressAccessor unknown{acc("score:/controls/elsewhere/Amount")};
    unknown.address.anchor = foreign.address.anchor;
    f.submit(new Scenario::Command::AddMessagesToState{
        state, State::MessageList{{unknown, 0.2f}}});
    auto kept = find(messages(state), unknown.address);
    REQUIRE(kept);
    REQUIRE(kept->address.address.anchor == foreign.address.anchor);
  });
}

TEST_CASE(
    "an explorer change reaches only the states holding addresses of its device",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& onDevice = f.startState();
    auto& itv = f.intervalAfterStart();
    auto& elsewhere = base_scenario(*f.doc).state(itv.endState());
    f.submit(new Scenario::Command::AddMessagesToState{
        onDevice, State::MessageList{{acc("testdev:/level"), 0.1f}}});
    f.submit(new Scenario::Command::AddMessagesToState{
        elsewhere, State::MessageList{{acc("nodevice:/a"), 0.2f}, {f.published, 0.3f}}});
    settle();

    auto& tree = f.dctx->plugin<LocalTree::DocumentPlugin>();
    std::vector<QObject*> notified;
    auto counting = QObject::connect(
        &tree, &LocalTree::ScriptableTreeBase::referencesChanged, &tree,
        [&](QObject* referrer) { notified.push_back(referrer); });
    auto wait = [] {
      QElapsedTimer t;
      t.start();
      while(t.elapsed() < 300)
        settle();
    };

    // A burst of changes on the device notifies its referrers once
    auto& level = f.deviceNode()->childAt(0);
    f.submit(new Explorer::Command::Remove{*f.devices, Device::NodePath{level}});
    f.doc->commandStack().undo();
    f.doc->commandStack().redo();
    wait();
    REQUIRE(notified == std::vector<QObject*>{&onDevice});

    // A device added or removed may concern any address of another device
    notified.clear();
    f.submit(new Explorer::Command::Remove{*f.devices, *f.deviceNode()});
    wait();
    REQUIRE(ossia::contains(notified, &onDevice));
    REQUIRE(ossia::contains(notified, &elsewhere));
    REQUIRE(!ossia::contains(notified, nullptr));
    QObject::disconnect(counting);
  });
}

TEST_CASE(
    "explorer nodes dropped on a state become messages to them",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& state = f.startState();
    auto& explorer = f.devices->explorer();
    std::unique_ptr<QMimeData> mime{explorer.mimeData(
        {explorer.modelIndexFromNode(*f.deviceNode(), 0)})};
    REQUIRE(mime);
    REQUIRE(mime->hasFormat(score::mime::messagelist()));
    state.messages().dropMimeData(mime.get(), Qt::CopyAction, -1, -1, {});

    auto ml = messages(state);
    REQUIRE(ml.size() == 2);
    REQUIRE(find(ml, acc("testdev:/level").address)->value == ossia::value{0.75f});
    REQUIRE(find(ml, acc("testdev:/other").address)->value == ossia::value{0.5f});
  });
}

TEST_CASE(
    "removing message nodes and clearing a state take any address",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& state = f.startState();
    const State::MessageList all{
        {f.published, 0.1f},
        {f.publishedFreq, 10.f},
        {acc("testdev:/level"), 0.2f},
        {acc("nodevice:/a"), 0.3f},
        {acc("score:/controls/*/Amount"), 0.4f}};
    f.submit(new Scenario::Command::AddMessagesToState{state, all});

    // Delete key on a published control and on a node of a missing device
    auto& model = state.messages();
    auto& amountNode
        = model.nodeFromModelIndex(indexOf(model, f.published.address, 0));
    auto& nodeviceNode = model.nodeFromModelIndex(indexOf(model, acc("nodevice:/a").address, 0));
    f.submit(new Scenario::Command::RemoveMessageNodes{
        state, {&amountNode, nodeviceNode.parent()}});
    auto ml = messages(state);
    REQUIRE(ml.size() == 3);
    REQUIRE(!find(ml, f.published.address));
    REQUIRE(find(ml, f.publishedFreq.address)->address.address.anchored());
    REQUIRE(!find(ml, acc("nodevice:/a").address));

    f.submit(new Scenario::Command::ClearState{state});
    REQUIRE(messages(state).empty());
    f.doc->commandStack().undo();
    f.doc->commandStack().undo();
    ml = messages(state);
    REQUIRE(ml.size() == all.size());
    REQUIRE(find(ml, f.published.address)->address.address.anchored());
  });
}

TEST_CASE(
    "find and replace moves messages to another control and anchors them there",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& state = f.startState();
    f.submit(new Scenario::Command::AddMessagesToState{
        state, State::MessageList{{f.published, 0.1f}, {acc("testdev:/level"), 0.2f}}});

    // Move the messages of a whole process at once
    auto other = score::test::add_process(*f.doc, smooth_uuid, {});
    REQUIRE(other);
    auto& otherAmount = control(*other, QStringLiteral("Amount"));
    f.submit(new Process::SetPortScriptable(otherAmount, true));
    settle();
    const auto otherAddress = LocalTree::scriptableAddress(otherAmount);
    REQUIRE(otherAddress != f.published.address);

    auto from = f.published.address;
    from.path.removeLast();
    from.anchor.reset();
    auto to = otherAddress;
    to.path.removeLast();
    f.submit(new Scenario::Command::RenameAddressesInState{state, from, to});
    settle();
    auto ml = messages(state);
    REQUIRE(ml.size() == 2);
    auto moved = find(ml, otherAddress);
    REQUIRE(moved);
    REQUIRE(moved->value == ossia::value{0.1f});

    // It follows the control it was moved to, not the previous one
    f.submit(new Process::RenameProcess{*f.proc, QStringLiteral("left")});
    settle();
    REQUIRE(find(messages(state), otherAddress));
    f.submit(new Process::RenameProcess{*other, QStringLiteral("arrived")});
    settle();
    REQUIRE(find(messages(state), LocalTree::scriptableAddress(otherAmount)));

    // Move a single message of a device shown in the explorer
    f.submit(new Scenario::Command::RenameAddressesInState{
        state, acc("testdev:/level").address, acc("testdev:/other").address});
    ml = messages(state);
    REQUIRE(ml.size() == 2);
    REQUIRE(!find(ml, acc("testdev:/level").address));
    REQUIRE(find(ml, acc("testdev:/other").address)->value == ossia::value{0.2f});
  });
}

TEST_CASE(
    "a sequence continues the published controls and explorer nodes of its start state",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    AutoSequence autoseq{ctx};
    auto& scenar = base_scenario(*f.doc);
    auto& state = f.startState();
    f.submit(new Scenario::Command::AddMessagesToState{
        state, State::MessageList{
                   {f.published, 0.1f},
                   {acc("testdev:/level"), 0.25f},
                   {acc("testdev:/nothing"), 0.2f},
                   {acc("nodevice:/a"), 0.3f},
                   {acc("score:/controls/*/Amount"), 0.4f}}});

    auto cmd = Scenario::Command::CreateSequence::make(
        *f.dctx, scenar, state.id(), TimeVal::fromMsecs(2000), 0.4);
    f.dctx->commandStack.push(cmd);
    settle();

    auto& end = scenar.state(cmd->createdState());
    auto ml = messages(end);
    auto published = find(ml, f.published.address);
    REQUIRE(published);
    REQUIRE(published->value == ossia::value{0.6f});
    REQUIRE(published->address.address.anchored());
    REQUIRE(find(ml, acc("testdev:/level").address)->value == ossia::value{0.75f});

    auto& itv = scenar.interval(cmd->createdInterval());
    auto amountAutom = f.automationOn(itv, f.published.address);
    REQUIRE(amountAutom);
    REQUIRE(f.automationOn(itv, acc("testdev:/level").address));

    f.doc->commandStack().undo();
    settle();
    REQUIRE(scenar.intervals.find(cmd->createdInterval()) == scenar.intervals.end());
  });
}

TEST_CASE(
    "interpolating states takes the range of a published control and of an explorer node",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto& scenar = base_scenario(*f.doc);
    auto& itv = f.intervalAfterStart();
    f.submit(new Scenario::Command::AddMessagesToState{
        f.startState(), State::MessageList{
                            {f.publishedFreq, 10.f},
                            {acc("testdev:/level"), 0.25f},
                            {acc("nodevice:/a"), 0.3f}}});
    f.submit(new Scenario::Command::AddMessagesToState{
        Scenario::endState(itv, scenar), State::MessageList{
                                             {f.publishedFreq, 200.f},
                                             {acc("testdev:/level"), 0.5f},
                                             {acc("nodevice:/a"), 0.6f}}});

    Scenario::Command::InterpolateStates({&itv}, f.dctx->commandStack);
    settle();
    auto freq = f.automationOn(itv, f.publishedFreq.address);
    REQUIRE(freq);
    REQUIRE(freq->min() == Catch::Approx(0.001));
    REQUIRE(freq->max() == Catch::Approx(300.));
    auto level = f.automationOn(itv, acc("testdev:/level").address);
    REQUIRE(level);
    REQUIRE(level->min() == Catch::Approx(0.));
    REQUIRE(level->max() == Catch::Approx(2.));
    auto nodev = f.automationOn(itv, acc("nodevice:/a").address);
    REQUIRE(nodev);
    REQUIRE(nodev->min() == Catch::Approx(0.3));
    REQUIRE(nodev->max() == Catch::Approx(0.6));

    // With the device removed, only the states hold the values
    f.doc->commandStack().undo();
    f.submit(new Explorer::Command::Remove{*f.devices, *f.deviceNode()});
    settle();
    Scenario::Command::InterpolateStates({&itv}, f.dctx->commandStack);
    level = f.automationOn(itv, acc("testdev:/level").address);
    REQUIRE(level);
    REQUIRE(level->min() == Catch::Approx(0.25));
  });
}

TEST_CASE(
    "the explorer selection is snapshotted into selected states and automated on "
    "selected intervals",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto widget = Explorer::findDeviceExplorerWidgetInstance(ctx);
    REQUIRE(widget);
    settle();

    auto& explorer = f.devices->explorer();
    auto& level = f.deviceNode()->childAt(0);
    auto sel = widget->view()->selectionModel();
    REQUIRE(sel);
    sel->setCurrentIndex(
        widget->proxyIndex(explorer.modelIndexFromNode(level, 0)),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    REQUIRE(explorer.selectedIndexes().size() > 0);

    auto& scenar = base_scenario(*f.doc);
    auto& itv = f.intervalAfterStart();
    auto& end = Scenario::endState(itv, scenar);
    f.submit(new Scenario::Command::AddMessagesToState{
        end, State::MessageList{{f.published, 0.1f}}});

    score::SelectionDispatcher{f.dctx->selectionStack}.select(end);
    settle();
    REQUIRE(end.selection.get());
    Scenario::SnapshotParametersInStates(*f.dctx);
    auto ml = messages(end);
    REQUIRE(ml.size() == 2);
    REQUIRE(find(ml, acc("testdev:/level").address)->value == ossia::value{0.75f});
    REQUIRE(find(ml, f.published.address)->address.address.anchored());

    f.submit(new Scenario::Command::AddMessagesToState{
        f.startState(), State::MessageList{{acc("testdev:/level"), 0.25f}}});
    Scenario::CreateCurves({&itv}, f.dctx->commandStack);
    settle();
    auto autom = f.automationOn(itv, acc("testdev:/level").address);
    REQUIRE(autom);
    REQUIRE(autom->min() <= 0.25);
    REQUIRE(autom->max() >= 0.75);
  });
}

TEST_CASE(
    "a row selected without moving the current index keeps the explorer usable",
    "[integration][state][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto widget = Explorer::findDeviceExplorerWidgetInstance(ctx);
    REQUIRE(widget);
    settle();

    auto& explorer = f.devices->explorer();
    auto& level = f.deviceNode()->childAt(0);
    auto sel = widget->view()->selectionModel();
    REQUIRE(sel);
    QAction* edit{};
    for(auto act : widget->findChildren<QAction*>())
      if(act->text() == QObject::tr("Edit"))
        edit = act;
    REQUIRE(edit);

    // Select programmatically, leaving the current index unset
    sel->setCurrentIndex({}, QItemSelectionModel::NoUpdate);
    sel->select(
        widget->proxyIndex(explorer.modelIndexFromNode(level, 0)),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    REQUIRE_NOTHROW(settle());
    REQUIRE(&explorer.nodeFromModelIndex(widget->view()->selectedIndex()) == &level);
    REQUIRE(edit->isEnabled());

    // The device row itself
    sel->setCurrentIndex({}, QItemSelectionModel::NoUpdate);
    sel->select(
        widget->proxyIndex(explorer.modelIndexFromNode(*f.deviceNode(), 0)),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    REQUIRE_NOTHROW(settle());
    REQUIRE(
        &explorer.nodeFromModelIndex(widget->view()->selectedIndex()) == f.deviceNode());

    f.submit(new Explorer::Command::Remove{*f.devices, *f.deviceNode()});
    REQUIRE_NOTHROW(settle());
  });
}

