// The scriptable namespace against devices, saved files and restored commands
#include <State/Address.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/State/MessageNode.hpp>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <LocalTree/ScriptableProcessComponent.hpp>
#include <LocalTree/ScriptableReference.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Commands/State/SnapshotProcess.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <score/command/CommandData.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <catch2/catch_all.hpp>

#include <thread>

namespace
{
const QString smooth_uuid = QStringLiteral("bf603921-5a48-4aa5-9bc1-48a762be6467");

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

Process::ControlInlet& control(Process::ProcessModel& p, const QString& name)
{
  for(auto inlet : p.inlets())
    if(auto ctl = qobject_cast<Process::ControlInlet*>(inlet); ctl && ctl->name() == name)
      return *ctl;
  FAIL("no control named " << name.toStdString());
  throw;
}

Process::ProcessModel& smooth(score::Document& doc)
{
  for(auto& p : score::test::base_interval(doc).processes)
    if(p.concreteKey() == UuidKey<Process::ProcessModel>::fromString(smooth_uuid))
      return p;
  FAIL("no smooth process");
  throw;
}

Scenario::StateModel& endState(score::Document& doc)
{
  return score::IDocument::get<Scenario::ScenarioDocumentModel>(doc).baseScenario().endState();
}

State::MessageList messages(const Scenario::StateModel& s)
{
  return Process::flatten(s.messages().rootNode());
}

Device::ProtocolFactory& protocol(const score::GUIApplicationContext& app, const QString& name)
{
  for(auto& f : app.interfaces<Device::ProtocolFactoryList>())
    if(f.prettyName() == name)
      return f;
  FAIL("no protocol " << name.toStdString());
  throw;
}

// A document with a published control and a message to it at the end
score::Document& published(const score::GUIApplicationContext& app)
{
  auto doc = score::test::new_document(app);
  auto proc = score::test::add_process(*doc, smooth_uuid, {});
  REQUIRE(proc);
  CommandDispatcher<> disp{doc->context().commandStack};
  disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
  disp.submit<Process::SetProcessScriptable>(*proc, true);
  settle();
  disp.submit<Scenario::Command::AddMessagesToState>(
      endState(*doc),
      State::MessageList{
          {State::AddressAccessor{LocalTree::scriptableAddress(control(*proc, "Amount"))},
           0.6f}});
  settle();
  return *doc;
}
}

TEST_CASE(
    "only the local device can be named score",
    "[integration][scriptable][devices]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto& explorer = doc->context().plugin<Explorer::DeviceDocumentPlugin>().explorer();
    auto osc = protocol(app, "OSC").defaultSettings();
    osc.name = "score";
    CHECK(!explorer.checkDeviceInstantiatable(osc));
    osc.name = "remote";
    CHECK(explorer.checkDeviceInstantiatable(osc));

    auto local = protocol(app, "Local").defaultSettings();
    local.name = "local";
    CHECK(!explorer.checkDeviceInstantiatable(local));
  });
}

TEST_CASE(
    "a device named score in a saved document is renamed on load, with its addresses",
    "[integration][scriptable][devices]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.path() + "/named.score";
    {
      auto& doc = published(app);
      auto& devices = doc.context().plugin<Explorer::DeviceDocumentPlugin>();
      // A device named as the local device of an older document
      auto osc = protocol(app, "OSC").defaultSettings();
      osc.name = "score";
      Device::Node dev{osc, nullptr};
      CommandDispatcher<>{doc.context().commandStack}.submit(
          new Explorer::Command::LoadDevice{devices, std::move(dev)});
      CommandDispatcher<>{doc.context().commandStack}.submit<Scenario::Command::AddMessagesToState>(
          endState(doc), State::MessageList{{*State::parseAddressAccessor("score:/level"), 0.25f}});
      settle();
      REQUIRE(app.docManager.saveDocumentAs(doc, path));
      app.docManager.forceCloseDocument(app, doc);
    }

    auto doc = app.docManager.loadFile(app, path);
    REQUIRE(doc);
    settle();
    auto& list = doc->context().plugin<Explorer::DeviceDocumentPlugin>().list();
    REQUIRE(list.localDevice());
    CHECK(list.localDevice()->name() == QStringLiteral("score"));
    REQUIRE(list.findDevice(QStringLiteral("score.1")));

    bool sawLocal = false, sawRemote = false;
    for(auto& m : messages(endState(*doc)))
    {
      if(m.address.address.path.startsWith(QStringLiteral("controls")))
      {
        CHECK(m.address.address.device == QStringLiteral("score"));
        sawLocal = true;
      }
      else
      {
        CHECK(m.address.address.device == QStringLiteral("score.1"));
        sawRemote = true;
      }
    }
    CHECK(sawLocal);
    CHECK(sawRemote);
    app.docManager.forceCloseDocument(app, *doc);
  });
}

TEST_CASE(
    "a local device saved under another name loads as score",
    "[integration][scriptable][devices]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.path() + "/local.score";
    {
      auto& doc = published(app);
      REQUIRE(app.docManager.saveDocumentAs(doc, path));
      app.docManager.forceCloseDocument(app, doc);
    }
    // The explorer of an older document shows the local device under its old name
    {
      QFile f{path};
      REQUIRE(f.open(QIODevice::ReadOnly));
      auto json = readJson(f.readAll());
      f.close();
      auto& alloc = json.GetAllocator();
      auto local = protocol(app, "Local").defaultSettings();
      local.name = "score (old)";
      JSONReader r;
      r.readFrom(Device::Node{local, nullptr});
      const auto node = readJson(r.toByteArray());
      bool added = false;
      for(auto& plug : json["Plugins"].GetArray())
        if(plug.HasMember("Children"))
        {
          plug["Children"].PushBack(rapidjson::Value(node, alloc), alloc);
          added = true;
        }
      REQUIRE(added);
      REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
      rapidjson::StringBuffer buf;
      rapidjson::Writer<rapidjson::StringBuffer> w{buf};
      json.Accept(w);
      f.write(buf.GetString(), buf.GetSize());
    }

    auto doc = app.docManager.loadFile(app, path);
    REQUIRE(doc);
    settle();
    auto& list = doc->context().plugin<Explorer::DeviceDocumentPlugin>().list();
    REQUIRE(list.localDevice());
    CHECK(list.localDevice()->name() == QStringLiteral("score"));
    auto& proc = smooth(*doc);
    CHECK(
        LocalTree::scriptableAddress(control(proc, "Amount")).device
        == QStringLiteral("score"));
    app.docManager.forceCloseDocument(app, *doc);
  });
}

TEST_CASE(
    "the local device forgets the listening requests of what it no longer publishes",
    "[integration][scriptable][devices]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto& doc = published(app);
    settle();
    auto& list = doc.context().plugin<Explorer::DeviceDocumentPlugin>().list();
    REQUIRE(list.localDevice());
    const auto published = list.localDevice()->listening().size();
    CommandDispatcher<>{doc.context().commandStack}.submit<Process::SetProcessScriptable>(
        smooth(doc), false);
    settle();
    CHECK(list.localDevice()->listening().size() < published);
  });
}

TEST_CASE(
    "anchored addresses with missing parts load as plain addresses",
    "[integration][scriptable][files]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    for(auto text : {R"({"Address":"score:/controls/fx/amount"})",
                     R"({"Address":"score:/controls/fx/amount","Target":5})",
                     R"({"Target":[]})", R"({"Address":3,"Target":[],"Member":4})"})
    {
      INFO(text);
      const auto json = readJson(QByteArray{text});
      State::AddressAccessor acc;
      JSONWriter w{json};
      w.writeTo(acc);
      CHECK(!acc.address.anchor);
    }
  });
}

TEST_CASE(
    "a binary save keeps anchors and scriptable flags",
    "[integration][scriptable][files]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.path() + "/binary.scorebin";
    {
      auto& doc = published(app);
      REQUIRE(messages(endState(doc)).at(0).address.address.anchor);
      REQUIRE(app.docManager.saveDocumentAs(doc, path));
      app.docManager.forceCloseDocument(app, doc);
    }
    auto doc = app.docManager.loadFile(app, path);
    REQUIRE(doc);
    settle();
    auto& proc = smooth(*doc);
    CHECK(proc.scriptable());
    const auto m = messages(endState(*doc)).at(0);
    REQUIRE(m.address.address.anchor);
    CHECK(m.address.address.anchor->resolve(doc->context()) == &control(proc, "Amount"));
    app.docManager.forceCloseDocument(app, *doc);
  });
}

TEST_CASE(
    "the commands of the scriptable namespace replay from their saved form",
    "[integration][scriptable][files]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto& doc = published(app);
    const auto& ctx = doc.context();
    auto& proc = smooth(doc);
    auto& amount = control(proc, "Amount");

    // Redoes then undoes a copy made from the saved form, as a crash restore does
    auto replay = [&](score::Command* cmd, auto check) {
      std::unique_ptr<score::Command> original{cmd};
      const score::CommandData data{*original};
      std::unique_ptr<score::Command> copy{app.components.instantiateUndoCommand(data)};
      REQUIRE(copy);
      copy->redo(ctx);
      settle();
      check(true);
      copy->undo(ctx);
      settle();
      check(false);
    };

    replay(new Process::SetPortScriptable{amount, true}, [&](bool done) {
      CHECK(amount.scriptable() == done);
    });
    replay(new Process::SetPortScriptingName{amount, QStringLiteral("wet")}, [&](bool done) {
      CHECK((amount.exposed() == QStringLiteral("wet")) == done);
    });
    replay(new Process::SetProcessScriptable{proc, false}, [&](bool done) {
      CHECK(proc.scriptable() == !done);
    });
    replay(new Process::RenameProcess{proc, QStringLiteral("reverb")}, [&](bool done) {
      CHECK((proc.metadata().getName() == QStringLiteral("reverb")) == done);
    });
    replay(new Process::SetValue{amount, ossia::value{0.9f}}, [&](bool done) {
      CHECK((amount.value() == ossia::value{0.9f}) == done);
    });
  });
}

namespace
{
// Replaces the last command by a copy made from its saved form, as a crash restore does
void replayLast(score::Document& doc)
{
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
}
}

TEST_CASE(
    "a snapshot of a process replays from its saved form",
    "[integration][scriptable][files]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    settle();
    auto& state = endState(*doc);
    Scenario::Command::snapshotProcessInState(state, *proc, doc->context());
    settle();
    const auto count = messages(state).size();
    REQUIRE(count > 0);

    replayLast(*doc);
    CHECK(proc->scriptable());
    CHECK(messages(state).size() == count);
    for(auto& m : messages(state))
      CHECK(m.address.address.anchor);
  });
}
