// A device node whose protocol this build cannot instantiate.
//
// This is not an exotic case: protocols are registered conditionally inside
// plug-ins that ship everywhere, so a document authored on macOS routinely
// names protocols a Windows build has no factory for. loadDeviceFromNode then
// keeps the node in the explorer with no DeviceInterface behind it -- and
// everything that reached for one through DeviceList::device() aborted, up to
// and including the model's data(), which runs on every repaint.

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Explorer/Column.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/plugins/UuidKey.hpp>

#include <core/document/Document.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
constexpr auto absent_uuid = "11111111-2222-3333-4444-555555555555";

Device::DeviceSettings ghostSettings(const QString& name = QStringLiteral("ghost"))
{
  Device::DeviceSettings s;
  s.protocol = UuidKey<Device::ProtocolFactory>::fromString(QString{absent_uuid});
  s.name = name;
  return s;
}

//! A device in the tree with nothing behind it, through the same path a load
//! takes: loadDeviceFromNode fails to find the factory and the node is kept.
Explorer::DeviceDocumentPlugin&
withGhostDevice(score::Document& doc, const QString& name = QStringLiteral("ghost"))
{
  auto& plug = doc.context().plugin<Explorer::DeviceDocumentPlugin>();
  plug.updateProxy.loadDevice(Device::Node{ghostSettings(name), nullptr});
  return plug;
}
}

TEST_CASE("A device with no factory stays in the tree unimplemented", "[explorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& plug = withGhostDevice(*doc);

    // The node is there...
    REQUIRE(plug.rootNode().childCount() >= 1);
    const auto& node = plug.rootNode().childAt(0);
    REQUIRE(node.is<Device::DeviceSettings>());
    CHECK(node.get<Device::DeviceSettings>().name == QStringLiteral("ghost"));

    // ... and nothing is behind it. Everything below depends on this: if a
    // factory did turn up, the test would be exercising the ordinary path.
    REQUIRE(plug.list().findDevice(QStringLiteral("ghost")) == nullptr);
  });
}

TEST_CASE("An unimplemented device can be displayed", "[explorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& plug = withGhostDevice(*doc);
    REQUIRE(plug.list().findDevice(QStringLiteral("ghost")) == nullptr);

    auto& model = plug.explorer();
    const auto idx = model.index(0, (int)Explorer::Column::Name, QModelIndex{});
    REQUIRE(idx.isValid());

    // data() asks the device whether it is connected. Reaching for one that is
    // not there aborted on every repaint of the explorer.
    const auto name = model.data(idx, Qt::DisplayRole);
    CHECK(name.toString().contains(QStringLiteral("ghost")));
  });
}

TEST_CASE("Addresses can be edited on an unimplemented device", "[explorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& plug = withGhostDevice(*doc);
    REQUIRE(plug.list().findDevice(QStringLiteral("ghost")) == nullptr);

    // The model still has to accept the edit: on a machine that lacks the
    // protocol -- or a terminal, which has no device implementations at all --
    // the document is what carries the address, and it has to survive a save.
    Device::AddressSettings addr;
    addr.name = QStringLiteral("param");

    Device::NodePath devicePath;
    devicePath.push_back(0);

    plug.updateProxy.addAddress(devicePath, addr, 0);

    REQUIRE(plug.rootNode().childCount() >= 1);
    const auto& device = plug.rootNode().childAt(0);
    REQUIRE(device.childCount() == 1);
    CHECK(device.childAt(0).displayName() == QStringLiteral("param"));

    // And removing it again.
    plug.updateProxy.removeNode(devicePath, addr);
    CHECK(plug.rootNode().childAt(0).childCount() == 0);
  });
}

TEST_CASE("An unimplemented device can be updated and removed", "[explorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& plug = withGhostDevice(*doc);
    REQUIRE(plug.list().findDevice(QStringLiteral("ghost")) == nullptr);

    auto renamed = ghostSettings(QStringLiteral("ghost"));
    renamed.deviceSpecificSettings = QVariant::fromValue(42);
    plug.updateProxy.updateDevice(QStringLiteral("ghost"), renamed);

    REQUIRE(plug.rootNode().childCount() >= 1);
    CHECK(
        plug.rootNode().childAt(0).get<Device::DeviceSettings>().deviceSpecificSettings
        == renamed.deviceSpecificSettings);

    plug.updateProxy.removeDevice(renamed);
    CHECK(plug.rootNode().childCount() == 0);
  });
}
