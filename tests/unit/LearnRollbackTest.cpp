// Cancelling a "Learn" must not take the device down with it.
//
// The rollback used to be ReloadWholeDevice::undo: remove the device, build a
// new one from the saved tree. Everything the outside world had attached to the
// old one went with it -- with PipeWire, cancelling a learn that had added
// nothing at all still dropped the user's MIDI cable.

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/LearnRollback.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <QApplication>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <set>
#include <string>

#include "FakeDeviceProtocol.hpp"

using namespace score::test::fake;

namespace
{
struct Fixture
{
  score::Document* doc{};
  Explorer::DeviceDocumentPlugin* plug{};

  explicit Fixture(const score::GUIApplicationContext& ctx)
  {
    g_opts = {};
    registerFakeProtocol(ctx);
    doc = score::test::new_document(ctx);
    REQUIRE(doc);
    plug = &doc->context().plugin<Explorer::DeviceDocumentPlugin>();
  }

  void addDevice(const Device::DeviceSettings& s)
  {
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Explorer::Command::LoadDevice{*plug, Device::Node{s, nullptr}});
  }

  Device::Node* deviceNode(const QString& name) const
  {
    for(auto& n : plug->rootNode())
      if(n.get<Device::DeviceSettings>().name == name)
        return &n;
    return nullptr;
  }

  std::set<std::string> tree(const QString& name) const
  {
    std::set<std::string> out;
    if(auto* n = deviceNode(name))
      for(auto& child : *n)
        out.insert(child.displayName().toStdString());
    return out;
  }

  Device::DeviceInterface* device(const QString& name) const
  {
    return plug->list().findDevice(name);
  }

  //! What a learn does: nodes appear in the device and in the explorer.
  void learn(const QString& deviceName, const QStringList& names)
  {
    auto* n = deviceNode(deviceName);
    REQUIRE(n);
    for(const auto& name : names)
    {
      Device::AddressSettings as;
      as.name = name;
      as.value = ossia::value{0};
      as.ioType = ossia::access_mode::BI;
      plug->updateProxy.addAddress(Device::NodePath{*n}, as, n->childCount());
    }
  }
};

using Names = std::set<std::string>;
}

TEST_CASE("cancelling a learn that found nothing leaves the device alone", "[learn]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    f.addDevice(fakeSettings("midi", {"note"}));

    auto* before = f.device("midi");
    REQUIRE(before);
    REQUIRE(before->connected());
    const int connects = g_opts.connectCount;

    // The learn saw nothing; the user pressed Cancel.
    const Device::Node saved = *f.deviceNode("midi");
    Explorer::rollbackLearnedNodes(*f.plug, "midi", saved);

    // Same device object, still connected, never reconnected: whatever was
    // wired to it outside score is still wired to it.
    CHECK(f.device("midi") == before);
    CHECK(f.device("midi")->connected());
    CHECK(g_opts.connectCount == connects);
    CHECK(f.tree("midi") == Names{"note"});
  });
}

TEST_CASE("cancelling a learn takes back what it added", "[learn]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    f.addDevice(fakeSettings("midi", {"note"}));

    auto* before = f.device("midi");
    REQUIRE(before);
    const Device::Node saved = *f.deviceNode("midi");
    const int connects = g_opts.connectCount;

    f.learn("midi", {"cc1", "cc2"});
    REQUIRE(f.tree("midi") == Names{"note", "cc1", "cc2"});

    Explorer::rollbackLearnedNodes(*f.plug, "midi", saved);

    CHECK(f.tree("midi") == Names{"note"});
    CHECK(f.device("midi") == before);
    CHECK(f.device("midi")->connected());
    CHECK(g_opts.connectCount == connects);
  });
}

TEST_CASE("cancelling a learn does not touch what was already there", "[learn]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    f.addDevice(fakeSettings("midi", {"note", "bend"}));

    const Device::Node saved = *f.deviceNode("midi");
    f.learn("midi", {"cc7"});

    Explorer::rollbackLearnedNodes(*f.plug, "midi", saved);
    CHECK(f.tree("midi") == Names{"note", "bend"});
  });
}

TEST_CASE("rolling back a device that is gone does nothing", "[learn]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    f.addDevice(fakeSettings("midi", {"note"}));
    const Device::Node saved = *f.deviceNode("midi");

    Explorer::rollbackLearnedNodes(*f.plug, "not-a-device", saved);
    SUCCEED("no crash");
  });
}
