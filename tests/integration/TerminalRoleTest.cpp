// A document opened as a terminal edits a score that runs on another machine.
//
// It must not claim this machine's hardware: no ports bound, no MIDI or camera
// taken, no render window opened on the wrong screen. All of that happens
// through device instantiation, which runs while the device plug-in is being
// deserialized -- so the role has to be known before the document is read,
// which is why it is fixed at construction rather than set afterwards.

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentRole.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/UuidKey.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/Menus/TransportActions.hpp>
#include <score/actions/ActionManager.hpp>
#include <Library/Panel/LibraryPanelDelegate.hpp>
#include <Library/ProcessWidget.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Process/ProcessList.hpp>
#include <Process/RemoteState.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <core/command/CommandStack.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>

#include <score_test/App.hpp>
#include <score_test/ProbeProtocol.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
using score::test::ProbeProtocolFactory;

Device::Node probeNode(const QString& name = QStringLiteral("probe"))
{
  return score::test::probe_device_node(name);
}

//! JSON, as a session sends it: RemoteClientBuilder writes saveAsJson and
//! ClientSessionBuilder reads it back with JSONObject::type(). The binary
//! format does not consult the device list on the way out, so saving that way
//! would exercise none of what a terminal does.
QByteArray asJson(score::Document& doc)
{
  JSONObject::Serializer wr{};
  doc.saveAsJson(wr);
  return wr.toByteArray();
}

//! A saved document holding one device that uses the probe protocol, plus one
//! address under it.
QByteArray documentWithProbeDevice(const score::GUIApplicationContext& ctx)
{
  auto* doc = score::test::new_document(ctx);
  SCORE_ASSERT(doc);

  auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
  plug.explorer().addDevice(probeNode());

  Device::AddressSettings addr;
  addr.name = QStringLiteral("param");
  Device::NodePath devicePath;
  devicePath.push_back(0);
  plug.updateProxy.addAddress(devicePath, addr, 0);

  return asJson(*doc);
}

score::Document* reload(
    const score::GUIApplicationContext& ctx, const QByteArray& bytes,
    score::DocumentRole role)
{
  auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
  SCORE_ASSERT(!delegates.empty());
  auto* doc = ctx.docManager.loadDocument(
      ctx, QStringLiteral("terminal"), bytes, JSONObject::type(), *delegates.begin(),
      role);
  QApplication::processEvents();
  return doc;
}
}

TEST_CASE("A local document builds the devices it names", "[terminal]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::test::register_probe_protocol(ctx);
    const auto bytes = documentWithProbeDevice(ctx);
    REQUIRE(bytes.size() > 0);

    ProbeProtocolFactory::requests = 0;
    auto* doc = reload(ctx, bytes, score::DocumentRole::Local);
    REQUIRE(doc);
    CHECK(doc->role() == score::DocumentRole::Local);

    // The precondition for the next test: loading normally does ask.
    CHECK(ProbeProtocolFactory::requests == 1);
  });
}

TEST_CASE("A terminal document builds no devices at all", "[terminal]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::test::register_probe_protocol(ctx);
    const auto bytes = documentWithProbeDevice(ctx);
    REQUIRE(bytes.size() > 0);

    ProbeProtocolFactory::requests = 0;
    auto* doc = reload(ctx, bytes, score::DocumentRole::Terminal);
    REQUIRE(doc);
    REQUIRE(doc->role() == score::DocumentRole::Terminal);

    CHECK(ProbeProtocolFactory::requests == 0);

    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    CHECK(plug.list().findDevice(QStringLiteral("probe")) == nullptr);
  });
}

TEST_CASE("A terminal still shows and keeps the score's devices", "[terminal]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::test::register_probe_protocol(ctx);
    const auto bytes = documentWithProbeDevice(ctx);

    auto* doc = reload(ctx, bytes, score::DocumentRole::Terminal);
    REQUIRE(doc);

    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();

    // The tree is what the person at the terminal is editing, so it has to be
    // all there: the device, and the addresses under it.
    REQUIRE(plug.rootNode().childCount() == 1);
    const auto& device = plug.rootNode().childAt(0);
    REQUIRE(device.is<Device::DeviceSettings>());
    CHECK(device.get<Device::DeviceSettings>().name == QStringLiteral("probe"));
    REQUIRE(device.childCount() == 1);
    CHECK(device.childAt(0).displayName() == QStringLiteral("param"));

    // And saving it again gives back a document naming the same device, so a
    // terminal is not a way to quietly lose what it could not instantiate.
    const auto resaved = asJson(*doc);
    REQUIRE(resaved.size() > 0);

    auto* again = reload(ctx, resaved, score::DocumentRole::Terminal);
    REQUIRE(again);
    auto& plug2 = again->context().plugin<Explorer::DeviceDocumentPlugin>();
    REQUIRE(plug2.rootNode().childCount() == 1);
    CHECK(
        plug2.rootNode().childAt(0).get<Device::DeviceSettings>().name
        == QStringLiteral("probe"));
    REQUIRE(plug2.rootNode().childAt(0).childCount() == 1);
  });
}

TEST_CASE("A terminal document does not execute", "[terminal]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::test::register_probe_protocol(ctx);
    const auto bytes = documentWithProbeDevice(ctx);

    auto* doc = reload(ctx, bytes, score::DocumentRole::Terminal);
    REQUIRE(doc);
    REQUIRE(doc->role() == score::DocumentRole::Terminal);

    // Play is a request to the host, never something this copy performs: it has
    // no devices to play through. Asking anyway must be declined rather than
    // start a graph against an empty device list.
    ctx.docManager.setCurrentDocument(ctx, doc);
    QApplication::processEvents();

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    engine.execution().request_play_global(true);
    QApplication::processEvents();
    QApplication::processEvents();

    auto* exec = doc->context().findPlugin<Execution::DocumentPlugin>();
    if(exec)
      CHECK_FALSE(exec->isPlaying());

    // Not asserted here: that the transport buttons stay honest. They are set
    // by TransportActions, which early-returns without the widgets a real
    // window creates, so headless it does nothing either way and the check
    // would pass with the guard removed. Verified in the GUI instead.
  });
}

TEST_CASE("A terminal exposes no control surface of its own", "[terminal]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::test::register_probe_protocol(ctx);
    const auto bytes = documentWithProbeDevice(ctx);

    // The local tree is score's own OSC/OSCQuery view of the document. On a
    // terminal it would be a second control surface for a score executing
    // somewhere else, bound to the same default ports as the machine actually
    // running it -- which collide outright when that is this machine.
    auto* terminal = reload(ctx, bytes, score::DocumentRole::Terminal);
    REQUIRE(terminal);
    auto& termDevices = terminal->context().plugin<Explorer::DeviceDocumentPlugin>();
    CHECK(termDevices.list().localDevice() == nullptr);

    // The precondition: an ordinary document does have one.
    auto* local = reload(ctx, bytes, score::DocumentRole::Local);
    REQUIRE(local);
    auto& localDevices = local->context().plugin<Explorer::DeviceDocumentPlugin>();
    CHECK(localDevices.list().localDevice() != nullptr);
  });
}

TEST_CASE("A terminal asks for the state of what it creates", "[terminal]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::test::register_probe_protocol(ctx);
    const auto bytes = documentWithProbeDevice(ctx);

    auto& facs = ctx.interfaces<Process::ProcessFactoryList>();
    REQUIRE_FALSE(facs.empty());
    const auto key = facs.begin()->concreteKey();

    auto makeProcess = [&](score::Document& doc) {
      auto& itv = safe_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
                      .baseScenario()
                      .interval();
      Process::awaitingRemoteState().clear();
      doc.context().document.commandStack().redoAndPush(
          new Scenario::Command::AddOnlyProcessToInterval{
              itv, key, QStringLiteral("/on/the/other/machine.isf"), QPointF{}});
      return Process::awaitingRemoteState().size();
    };

    // A document that runs here builds the process from the same data the
    // command carries and has no reason to ask anyone.
    auto* local = reload(ctx, bytes, score::DocumentRole::Local);
    REQUIRE(local);
    CHECK(makeProcess(*local) == 0);

    // A terminal cannot: the data describes the other machine -- a library
    // entry carries the path of the file it was scanned from -- so the factory
    // succeeds here and produces something empty. What it should contain is
    // only known where the command came from.
    auto* terminal = reload(ctx, bytes, score::DocumentRole::Terminal);
    REQUIRE(terminal);
    CHECK(makeProcess(*terminal) == 1);

    Process::awaitingRemoteState().clear();
  });
}

TEST_CASE("A score keeps playing until asked, whatever document is in front",
          "[terminal]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::test::register_probe_protocol(ctx);
    const auto bytes = documentWithProbeDevice(ctx);

    // Both up front: loading a document stops execution (prepareNewDocument),
    // so creating the terminal later would stop the score for the wrong reason
    // and the test would pass without the fix.
    auto* local = reload(ctx, bytes, score::DocumentRole::Local);
    REQUIRE(local);
    auto* terminal = reload(ctx, bytes, score::DocumentRole::Terminal);
    REQUIRE(terminal);

    ctx.docManager.setCurrentDocument(ctx, local);
    QApplication::processEvents();

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    engine.execution().request_play_global(true);
    QApplication::processEvents();
    QApplication::processEvents();

    auto* exec = local->context().findPlugin<Execution::DocumentPlugin>();
    REQUIRE(exec);
    REQUIRE(exec->isPlaying());

    // Selecting a terminal does not stop what is running -- it belongs to the
    // document that started it, and there is only one execution controller.
    ctx.docManager.setCurrentDocument(ctx, terminal);
    QApplication::processEvents();
    CHECK(exec->isPlaying());

    // And Stop must still reach it. Refusing because the document in front is
    // a terminal leaves a score playing with no way to stop it.
    engine.execution().request_stop();
    QApplication::processEvents();
    QApplication::processEvents();

    CHECK_FALSE(exec->isPlaying());
  });
}
