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
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
constexpr auto probe_uuid = "9f1c0f4a-1b2c-4d3e-8f70-0badc0ffee00";

//! Counts the requests to build a device rather than building one. Returning
//! null is a case the explorer already handles (it is what a protocol whose
//! hardware is absent does), so the count is the only thing under test: a
//! terminal must not get this far at all.
struct ProbeProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("9f1c0f4a-1b2c-4d3e-8f70-0badc0ffee00")
public:
  static inline int requests = 0;

  QString prettyName() const noexcept override { return QStringLiteral("Probe"); }
  QString category() const noexcept override { return StandardCategories::util; }

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings&, const Explorer::DeviceDocumentPlugin&,
      const score::DocumentContext&) override
  {
    ++requests;
    return nullptr;
  }

  const Device::DeviceSettings& defaultSettings() const noexcept override
  {
    static const Device::DeviceSettings s = [] {
      Device::DeviceSettings d;
      d.protocol = static_concreteKey();
      d.name = QStringLiteral("Probe");
      return d;
    }();
    return s;
  }

  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface&, const score::DocumentContext&, QWidget*) override
  {
    return nullptr;
  }
  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&, const Device::DeviceInterface&,
      const score::DocumentContext&, QWidget*) override
  {
    return nullptr;
  }
  Device::ProtocolSettingsWidget* makeSettingsWidget() override { return nullptr; }

  QVariant makeProtocolSpecificSettings(const VisitorVariant&) const override
  {
    return {};
  }
  void serializeProtocolSpecificSettings(const QVariant&, const VisitorVariant&)
      const override
  {
  }
  bool checkCompatibility(
      const Device::DeviceSettings&, const Device::DeviceSettings&) const noexcept override
  {
    return true;
  }
};

//! Registered into the running application: the point is a protocol this build
//! genuinely has, so that not building a device is a decision rather than an
//! absence.
void registerProbe(const score::GUIApplicationContext& ctx)
{
  auto& list = ctx.interfaces<Device::ProtocolFactoryList>();
  if(list.get(ProbeProtocolFactory::static_concreteKey()))
    return;
  const_cast<Device::ProtocolFactoryList&>(list).insert(
      std::make_unique<ProbeProtocolFactory>());
}

Device::Node probeNode(const QString& name = QStringLiteral("probe"))
{
  Device::DeviceSettings s;
  s.protocol = ProbeProtocolFactory::static_concreteKey();
  s.name = name;
  return Device::Node{s, nullptr};
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
    registerProbe(ctx);
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
    registerProbe(ctx);
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
    registerProbe(ctx);
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
    registerProbe(ctx);
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
  });
}
