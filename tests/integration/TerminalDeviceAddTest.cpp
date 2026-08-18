// Adding a device whose protocol this build does not have.
//
// That is the ordinary case on a terminal: the score runs on the other machine,
// which has the evdev, the MIDI ports and the cameras. The dialog offers that
// machine's protocols and the hardware it enumerated, and the device is created
// over there. Nothing here can build a settings form for it -- the form is C++
// in a plug-in we do not have -- so the settings that came across are the whole
// of what we know, and dropping them means such a device can never be added.

#include <Device/Address/AddressSettings.hpp>
#include <Device/Protocol/DeviceCatalog.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/Widgets/DeviceEditDialog.hpp>

#include <score/plugins/InterfaceList.hpp>

#include <core/document/Document.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <QTreeWidget>

#include <catch2/catch_test_macros.hpp>

namespace
{
constexpr auto absent_uuid = "c0ffee00-1111-2222-3333-444455556666";

UuidKey<Device::ProtocolFactory> absentProtocol()
{
  return UuidKey<Device::ProtocolFactory>::fromString(QString{absent_uuid});
}

//! The other machine's protocols and hardware. "Absent" is one this build has
//! no factory for, which is the case under test; "Barren" is one that
//! enumerates nothing, like OSC or MQTT.
struct OtherMachine final : Device::DeviceCatalog
{
  QString enumeratedName{"Keyboard"};
  bool answerDevices{true};

  std::vector<Protocol> protocols() const override
  {
    return {
        Protocol{absentProtocol(), "Evdev-like", "Input", false},
        Protocol{
            UuidKey<Device::ProtocolFactory>::fromString(
                QString{"dead0000-1111-2222-3333-444455556666"}),
            "Barren", "Network", false}};
  }

  void enumerate(const UuidKey<Device::ProtocolFactory>& protocol, OnDevice onDevice)
      override
  {
    if(!answerDevices || protocol != absentProtocol())
      return;

    Device::DeviceSettings s;
    s.protocol = absentProtocol();
    s.name = enumeratedName;
    onDevice("Devices", enumeratedName, s);
  }
};

Device::DeviceSettings enumeratedSettings()
{
  Device::DeviceSettings s;
  s.protocol = absentProtocol();
  s.name = "Keyboard";
  return s;
}
}

TEST_CASE("A device whose protocol only the other machine has can be added", "[devices]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    // Not something this build can make: that is the whole point.
    REQUIRE(!ctx.interfaces<Device::ProtocolFactoryList>().get(absentProtocol()));

    OtherMachine catalog;
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    plug.setCatalog(&catalog);

    auto& explorer = Explorer::deviceExplorerFromContext(doc->context());

    // Refusing here is what made every such device unaddable: the check asked
    // this machine for a factory, and the device is made on the other one.
    CHECK(explorer.checkDeviceInstantiatable(enumeratedSettings()));

    // A protocol nobody offers is still refused.
    Device::DeviceSettings unknown;
    unknown.name = "nope";
    unknown.protocol
        = UuidKey<Device::ProtocolFactory>::fromString(QString{"11112222-3333-4444-5555-666677778888"});
    CHECK_FALSE(explorer.checkDeviceInstantiatable(unknown));

    plug.setCatalog(nullptr);
  });
}

TEST_CASE("The settings the other machine sent are what gets added", "[devices]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    OtherMachine catalog;
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    plug.setCatalog(&catalog);

    auto& explorer = Explorer::deviceExplorerFromContext(doc->context());
    Explorer::DeviceEditDialog dial{
        explorer, ctx.interfaces<Device::ProtocolFactoryList>(),
        Explorer::DeviceEditDialog::Creating, nullptr};

    dial.setSettings(enumeratedSettings());

    // There is no widget to hold them -- the form lives in a plug-in this build
    // does not have -- so the dialog itself has to remember them. Answering
    // with an empty DeviceSettings is how the device got lost.
    const auto out = dial.getSettings();
    CHECK(out.protocol == absentProtocol());
    CHECK(out.name == "Keyboard");

    plug.setCatalog(nullptr);
  });
}

TEST_CASE("A protocol that enumerates nothing shows no device list", "[devices]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    OtherMachine catalog;
    catalog.answerDevices = false;
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    plug.setCatalog(&catalog);

    auto& explorer = Explorer::deviceExplorerFromContext(doc->context());
    Explorer::DeviceEditDialog dial{
        explorer, ctx.interfaces<Device::ProtocolFactoryList>(),
        Explorer::DeviceEditDialog::Creating, nullptr};

    dial.setSettings(enumeratedSettings());

    // Most protocols have nothing plugged into them. A column headed "Devices"
    // that is permanently empty says the other machine has none, which is a
    // different claim from not asking.
    //
    // isHidden, not isVisible: nothing is visible in a dialog that was never
    // shown, so isVisible() would pass here whatever the code did.
    auto* devices = dial.findChild<QTreeWidget*>("DeviceList");
    REQUIRE(devices);
    CHECK(devices->isHidden());

    plug.setCatalog(nullptr);
  });
}

TEST_CASE("A protocol with hardware behind it shows the list", "[devices]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    OtherMachine catalog;
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    plug.setCatalog(&catalog);

    auto& explorer = Explorer::deviceExplorerFromContext(doc->context());
    Explorer::DeviceEditDialog dial{
        explorer, ctx.interfaces<Device::ProtocolFactoryList>(),
        Explorer::DeviceEditDialog::Creating, nullptr};

    dial.setSettings(enumeratedSettings());

    auto* devices = dial.findChild<QTreeWidget*>("DeviceList");
    REQUIRE(devices);
    CHECK_FALSE(devices->isHidden());

    // The category heading, and the keyboard under it.
    REQUIRE(devices->topLevelItemCount() == 1);
    REQUIRE(devices->topLevelItem(0)->childCount() == 1);
    CHECK(devices->topLevelItem(0)->child(0)->text(0) == "Keyboard");

    plug.setCatalog(nullptr);
  });
}

TEST_CASE("Picking the other machine's hardware carries its settings", "[devices]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    OtherMachine catalog;
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    plug.setCatalog(&catalog);

    auto& explorer = Explorer::deviceExplorerFromContext(doc->context());
    Explorer::DeviceEditDialog dial{
        explorer, ctx.interfaces<Device::ProtocolFactoryList>(),
        Explorer::DeviceEditDialog::Creating, nullptr};

    // The real path: choose the protocol, then click what is plugged into the
    // other machine. Shown, because the dialog ignores clicks on a column it
    // believes is not on screen.
    dial.setSettings(enumeratedSettings());
    dial.show();

    auto* devices = dial.findChild<QTreeWidget*>("DeviceList");
    REQUIRE(devices);
    REQUIRE(devices->topLevelItemCount() == 1);
    auto* item = devices->topLevelItem(0)->child(0);
    REQUIRE(item);

    devices->setCurrentItem(item);
    devices->activated(devices->currentIndex());

    const auto out = dial.getSettings();
    CHECK(out.protocol == absentProtocol());
    CHECK(out.name == "Keyboard");

    plug.setCatalog(nullptr);
  });
}

TEST_CASE("The device handed to the add command is not empty", "[devices]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    OtherMachine catalog;
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    plug.setCatalog(&catalog);

    auto& explorer = Explorer::deviceExplorerFromContext(doc->context());
    Explorer::DeviceEditDialog dial{
        explorer, ctx.interfaces<Device::ProtocolFactoryList>(),
        Explorer::DeviceEditDialog::Creating, nullptr};

    dial.setSettings(enumeratedSettings());

    // What the add path actually reads -- not getSettings(). An empty node here
    // is a null target, which the caller dereferenced on its way to adding
    // nothing: Add was clickable and did nothing at all.
    auto node = dial.getDevice();
    auto* settings = node.target<Device::DeviceSettings>();
    REQUIRE(settings);
    CHECK(settings->protocol == absentProtocol());
    CHECK(settings->name == "Keyboard");

    // And it must be something the model will accept, or it is dropped one
    // step later.
    CHECK(explorer.checkDeviceInstantiatable(*settings));

    plug.setCatalog(nullptr);
  });
}

TEST_CASE("Nothing chosen yields no device rather than a broken one", "[devices]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    OtherMachine catalog;
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    plug.setCatalog(&catalog);

    auto& explorer = Explorer::deviceExplorerFromContext(doc->context());
    Explorer::DeviceEditDialog dial{
        explorer, ctx.interfaces<Device::ProtocolFactoryList>(),
        Explorer::DeviceEditDialog::Creating, nullptr};

    // Nothing selected: an empty node is right, and the caller has to cope
    // with it rather than read through it.
    auto node = dial.getDevice();
    CHECK(node.target<Device::DeviceSettings>() == nullptr);

    plug.setCatalog(nullptr);
  });
}

TEST_CASE("A device arriving with a tree announces it", "[devices]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto& explorer = Explorer::deviceExplorerFromContext(doc->context());
    (void)explorer;

    QStringList announced;
    QObject::connect(
        &plug, &Explorer::DeviceDocumentPlugin::deviceTreeChanged, &plug,
        [&](const QString& name) { announced.push_back(name); });

    // What the machine running the score ends up with after opening a device:
    // the node it was asked for, plus whatever the thing turned out to contain.
    // The peer that asked sent none of this -- it cannot even make the device.
    Device::DeviceSettings s;
    s.protocol = absentProtocol();
    s.name = "Mouse";

    Device::Node node{s, nullptr};
    Device::AddressSettings axis;
    axis.name = "x";
    node.emplace_back(axis, &node);

    plug.updateProxy.addDevice(node);

    // Announced, because the command that created it carried a device with
    // nothing inside: what is under it was discovered here, and a peer that
    // never hears about it shows an empty device forever.
    CHECK(announced.contains("Mouse"));
  });
}
