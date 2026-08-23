// When the device explorer re-explores a device's namespace, and what it keeps.
//
// Adding a device, editing its settings, removing it and undoing that: the tree
// shown for the device must end up being what the device actually has, while
// nodes that are user data (learned addresses, custom nodes, a tree loaded from
// a file) survive all of it.
//
// The protocol under test is a fake whose "remote namespace" is the list of
// names stored in its device-specific settings, so that editing the settings
// is editing what the device will find when it reconnects.

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Remove.hpp>
#include <Explorer/Commands/Update/UpdateDeviceSettings.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/plugins/Interface.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/network/base/parameter.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QPointer>
#include <QStringList>
#include <QTimer>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <memory>
#include <set>
#include <string>

#include "FakeDeviceProtocol.hpp"

using namespace score::test::fake;

namespace
{
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  } while(t.elapsed() < ms);
}

//! Knobs of the fake protocol, shared by all its devices.

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

  const score::DocumentContext& context() const { return doc->context(); }

  void submit(score::Command* cmd)
  {
    CommandDispatcher<>{context().commandStack}.submit(cmd);
  }
  void undo() { context().document.commandStack().undoQuiet(); }
  void redo() { context().document.commandStack().redoQuiet(); }

  const Device::Node* deviceNode(const QString& name) const
  {
    for(auto& n : plug->rootNode())
      if(n.get<Device::DeviceSettings>().name == name)
        return &n;
    return nullptr;
  }

  //! The names of the device's nodes in the explorer
  std::set<std::string> tree(const QString& name) const
  {
    std::set<std::string> out;
    if(auto n = deviceNode(name))
      for(auto& child : *n)
        out.insert(child.displayName().toStdString());
    return out;
  }

  Device::DeviceInterface* device(const QString& name) const
  {
    return plug->list().findDevice(name);
  }

  void addDevice(Device::Node node)
  {
    submit(new Explorer::Command::LoadDevice{*plug, std::move(node)});
  }
  void addDevice(const Device::DeviceSettings& s) { addDevice(Device::Node{s, nullptr}); }

  void editDevice(const QString& name, const Device::DeviceSettings& s)
  {
    submit(new Explorer::Command::UpdateDeviceSettings{*plug, name, s});
  }

  void removeDevice(const QString& name)
  {
    auto n = deviceNode(name);
    REQUIRE(n);
    submit(new Explorer::Command::Remove{*plug, *n});
  }
};

using Names = std::set<std::string>;
}

TEST_CASE("Adding a device explores its namespace", "[deviceexplorer][reload]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};

    f.addDevice(fakeSettings("cam", {"pan", "tilt"}));

    CHECK(f.tree("cam") == Names{"pan", "tilt"});
    CHECK(g_opts.refreshCount == 1);
    REQUIRE(f.device("cam"));
    CHECK(f.device("cam")->connected());
  });
}

TEST_CASE("Adding a device from a node replays the node's tree before exploring", "[deviceexplorer][reload]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};

    // A .device file, a preset, the saved tree of a removed device: the nodes
    // it carries are user data for a protocol that keeps its tree.
    Device::Node n{fakeSettings("midi", {"note"}), nullptr};
    n.push_back(leaf("learned_cc"));
    f.addDevice(n);

    CHECK(f.tree("midi") == Names{"note", "learned_cc"});
  });
}

TEST_CASE("A device that only explores its namespace does not replay stale nodes", "[deviceexplorer][reload]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    g_opts.canSerialize = false;

    Device::Node n{fakeSettings("remote", {"live"}), nullptr};
    n.push_back(leaf("stale"));
    f.addDevice(n);

    CHECK(f.tree("remote") == Names{"live"});
  });
}

TEST_CASE("Adding a device that is not reachable keeps the tree it came with", "[deviceexplorer][reload]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    g_opts.available = false;

    Device::Node n{fakeSettings("down", {"pan"}), nullptr};
    n.push_back(leaf("saved"));
    f.addDevice(n);

    // Not wiped by an empty exploration
    CHECK(f.tree("down") == Names{"saved"});
    REQUIRE(f.device("down"));
    CHECK(!f.device("down")->connected());
  });
}

TEST_CASE("Undoing the removal of a device keeps its learned nodes", "[deviceexplorer][reload]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};

    Device::Node n{fakeSettings("midi", {"note"}), nullptr};
    n.push_back(leaf("learned_cc"));
    f.addDevice(n);
    REQUIRE(f.tree("midi") == Names{"note", "learned_cc"});

    f.removeDevice("midi");
    CHECK(f.deviceNode("midi") == nullptr);
    CHECK(f.device("midi") == nullptr);

    f.undo();
    CHECK(f.tree("midi") == Names{"note", "learned_cc"});

    f.redo();
    CHECK(f.deviceNode("midi") == nullptr);
    f.undo();
    CHECK(f.tree("midi") == Names{"note", "learned_cc"});
  });
}

TEST_CASE("Editing a device's settings re-explores its namespace", "[deviceexplorer][reload]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};

    f.addDevice(fakeSettings("cam", {"pan"}));
    REQUIRE(f.tree("cam") == Names{"pan"});
    const int refreshes = g_opts.refreshCount;

    // Point the device somewhere else: what it finds there shows up
    f.editDevice("cam", fakeSettings("cam", {"zoom", "focus"}));
    spin(20);
    auto t = f.tree("cam");
    CHECK(t.count("zoom") == 1);
    CHECK(t.count("focus") == 1);
    CHECK(g_opts.refreshCount > refreshes);
  });
}

TEST_CASE("Undoing / redoing a device edit re-explores its namespace", "[deviceexplorer][reload]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};

    f.addDevice(fakeSettings("cam", {"pan"}));
    f.editDevice("cam", fakeSettings("cam", {"zoom", "focus"}));
    spin(20);
    REQUIRE(f.tree("cam").count("zoom") == 1);

    {
      f.undo();
      spin(20);
      CHECK(f.tree("cam").count("pan") == 1);
      CHECK(f.device("cam")->settings().deviceSpecificSettings.toStringList()
            == QStringList{"pan"});

      f.redo();
      spin(20);
      CHECK(f.tree("cam").count("zoom") == 1);
      CHECK(f.tree("cam").count("focus") == 1);
    }
  });
}

TEST_CASE("Fast undo / redo of a device edit ends up consistent", "[deviceexplorer][reload]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};

    f.addDevice(fakeSettings("cam", {"pan"}));
    f.editDevice("cam", fakeSettings("cam", {"zoom", "focus"}));
    spin(20);
    REQUIRE(f.tree("cam").count("zoom") == 1);

    {
      for(int i = 0; i < 5; i++)
      {
        f.undo();
        f.redo();
      }
      f.undo();
      spin(40);
      CHECK(f.tree("cam").count("pan") == 1);
      CHECK(f.device("cam")->settings().deviceSpecificSettings.toStringList()
            == QStringList{"pan"});
      f.redo();
      spin(40);
      CHECK(f.tree("cam").count("zoom") == 1);
    }
  });
}

TEST_CASE("Editing a device that connects later re-explores once connected", "[deviceexplorer][reload]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};

    f.addDevice(fakeSettings("cam", {"pan"}));
    REQUIRE(f.tree("cam") == Names{"pan"});

    g_opts.deferConnect = true;
    f.editDevice("cam", fakeSettings("cam", {"zoom"}));
    // Not connected yet: nothing to show but what we had
    CHECK(f.tree("cam").count("zoom") == 0);

    spin(40);
    CHECK(f.tree("cam").count("zoom") == 1);
  });
}

TEST_CASE("Editing a device that cannot explore its namespace leaves its tree alone", "[deviceexplorer][reload]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    g_opts.canRefreshTree = false;

    Device::Node n{fakeSettings("osc", {}), nullptr};
    n.push_back(leaf("learned"));
    f.addDevice(n);
    REQUIRE(f.tree("osc") == Names{"learned"});

    f.editDevice("osc", fakeSettings("osc", {"ignored"}));
    spin(20);
    CHECK(f.tree("osc") == Names{"learned"});
    CHECK(g_opts.refreshCount == 0);
  });
}

TEST_CASE("Removing a device before it reconnects is safe", "[deviceexplorer][reload]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};

    f.addDevice(fakeSettings("cam", {"pan"}));
    g_opts.deferConnect = true;
    f.editDevice("cam", fakeSettings("cam", {"zoom"}));
    f.removeDevice("cam");
    spin(40);
    CHECK(f.deviceNode("cam") == nullptr);
    SUCCEED("no refresh on a removed device");
  });
}

TEST_CASE("An exploration that yields nothing never replaces the tree", "[deviceexplorer][reload]")
{
  // A slow host whose namespace does not make it within refresh()'s timeout
  // answers with an empty tree, indistinguishable from an empty namespace:
  // the tree we have is worth more than nothing.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};

    f.addDevice(fakeSettings("cam", {"pan"}));
    REQUIRE(f.tree("cam") == Names{"pan"});

    // What is shown is listened to...
    auto dev = f.device("cam");
    REQUIRE(dev);
    State::Address pan;
    pan.device = "cam";
    pan.path = QStringList{"pan"};
    dev->setListening(pan, true);
    REQUIRE(dev->listening().size() == 1);

    // Edit towards a host that answers nothing
    f.editDevice("cam", fakeSettings("cam", {}));
    spin(20);
    CHECK(f.device("cam")->connected());
    CHECK(f.tree("cam") == Names{"pan"});
    // ... and still is: the node was replayed into the new device
    CHECK(dev->listening().size() == 1);
  });
}

// ---- Listening --------------------------------------------------------------
//
// What the explorer asked to listen to must stay listened to across everything
// that rebuilds the device's nodes: editing its settings (reconnect + replay),
// re-exploring its namespace, a request made while it was disconnected.

namespace
{
State::Address addr(const QString& device, const QString& name)
{
  State::Address a;
  a.device = device;
  a.path = QStringList{name};
  return a;
}

//! The values the device reported for `name`, through its listening callback.
struct ValueSpy : Nano::Observer
{
  std::vector<ossia::value> values;
  explicit ValueSpy(Device::DeviceInterface& dev, const QString& name)
  {
    dev.valueUpdated.connect<&ValueSpy::on_value>(*this);
    m_name = name;
  }
  void on_value(const State::Address& a, const ossia::value& v)
  {
    if(a.path.size() == 1 && a.path[0] == m_name)
      values.push_back(v);
  }
  QString m_name;
};

void pushRemoteValue(Device::DeviceInterface& dev, const QString& name, float v)
{
  auto dev_base = dev.getDevice();
  REQUIRE(dev_base);
  auto node = ossia::net::find_node(dev_base->get_root_node(), name.toStdString());
  REQUIRE(node);
  REQUIRE(node->get_parameter());
  node->get_parameter()->push_value(v);
}

bool listens(Device::DeviceInterface& dev, const QString& name)
{
  for(const auto& a : dev.listening())
    if(a.path.size() == 1 && a.path[0] == name)
      return true;
  return false;
}
}

TEST_CASE("Listening survives editing the device's settings", "[deviceexplorer][reload][listening]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    f.addDevice(fakeSettings("cam", {"pan", "tilt"}));
    auto dev = f.device("cam");
    REQUIRE(dev);

    dev->setListening(addr("cam", "pan"), true);
    REQUIRE(listens(*dev, "pan"));
    ValueSpy spy{*dev, "pan"};
    pushRemoteValue(*dev, "pan", 0.25f);
    REQUIRE(spy.values.size() >= 1);

    // Edit: the device reconnects with new nodes, one of them still "pan"
    f.editDevice("cam", fakeSettings("cam", {"pan", "zoom"}));
    spin(30);
    REQUIRE(f.tree("cam").count("zoom") == 1);

    CHECK(listens(*dev, "pan"));
    const auto before = spy.values.size();
    pushRemoteValue(*dev, "pan", 0.75f);
    CHECK(spy.values.size() > before);
    CHECK(spy.values.back() == ossia::value{0.75f});

    // Not listened to: what was never asked for
    CHECK(!listens(*dev, "zoom"));

    // Undo: again a rebuild, "pan" still there
    f.undo();
    spin(30);
    CHECK(listens(*dev, "pan"));
  });
}

TEST_CASE("Listening requested while disconnected applies on reconnection", "[deviceexplorer][reload][listening]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    g_opts.available = false;
    f.addDevice(fakeSettings("cam", {"pan"}));
    auto dev = f.device("cam");
    REQUIRE(dev);
    REQUIRE(!dev->connected());

    dev->setListening(addr("cam", "pan"), true);
    CHECK(!listens(*dev, "pan"));
    CHECK(dev->listeningRequests().size() == 1);

    // The host comes up and the device is reconnected (the "reconnect" action
    // of the explorer: reconnect, replay, restore)
    g_opts.available = true;
    dev->reconnect();
    dev->recreate(*f.deviceNode("cam"));
    dev->restoreListening();
    CHECK(listens(*dev, "pan"));
  });
}

TEST_CASE("Stopping listening forgets the request", "[deviceexplorer][reload][listening]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    f.addDevice(fakeSettings("cam", {"pan"}));
    auto dev = f.device("cam");
    REQUIRE(dev);

    dev->setListening(addr("cam", "pan"), true);
    dev->setListening(addr("cam", "pan"), false);
    CHECK(!listens(*dev, "pan"));
    CHECK(dev->listeningRequests().empty());

    // A rebuild does not bring it back
    f.editDevice("cam", fakeSettings("cam", {"pan"}));
    spin(30);
    CHECK(!listens(*dev, "pan"));
  });
}
