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
struct FakeOptions
{
  bool canRefreshTree{true};
  bool canSerialize{true};
  //! The device connects (false: like an OSCQuery host that is down).
  bool available{true};
  //! The device connects from the event loop rather than right away, like the
  //! protocols which resolve their host in a thread.
  bool deferConnect{false};

  int refreshCount{};
  int connectCount{};
};
FakeOptions g_opts;

QStringList remoteOf(const Device::DeviceSettings& s)
{
  return s.deviceSpecificSettings.toStringList();
}

class FakeDevice final : public Device::OwningDeviceInterface
{
public:
  explicit FakeDevice(const Device::DeviceSettings& s)
      : OwningDeviceInterface{s}
  {
    m_capas.canRefreshTree = g_opts.canRefreshTree;
    m_capas.canSerialize = g_opts.canSerialize;
    m_capas.canLearn = true;
  }

  bool reconnect() override
  {
    disconnect();
    if(!g_opts.available)
    {
      connectionChanged(false);
      return false;
    }

    if(g_opts.deferConnect)
    {
      QTimer::singleShot(0, this, [self = QPointer{this}] {
        if(self)
          self->connectNow();
      });
      return false;
    }

    connectNow();
    return true;
  }

  void connectNow()
  {
    g_opts.connectCount++;
    auto dev = new ossia::net::generic_device{
        std::make_unique<ossia::net::multiplex_protocol>(),
        settings().name.toStdString()};
    for(const auto& name : remoteOf(settings()))
      ossia::create_parameter(dev->get_root_node(), name.toStdString(), "float");

    replaceDevice(dev);
    connectionChanged(true);
  }

  void recreate(const Device::Node& n) override
  {
    for(auto& child : n)
      addNode(child);
  }

  Device::Node refresh() override
  {
    g_opts.refreshCount++;
    if(!connected())
      return Device::Node{settings(), nullptr};
    return simple_refresh();
  }
};

class FakeSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  Device::DeviceSettings m_settings;
  Device::DeviceSettings getSettings() const override { return m_settings; }
  void setSettings(const Device::DeviceSettings& s) override { m_settings = s; }
};

class FakeFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("0b1f4c6e-2d3a-4e5f-8a9b-7c6d5e4f3a2b")
public:
  QString prettyName() const noexcept override { return QStringLiteral("Fake"); }
  QString category() const noexcept override { return QStringLiteral("Test"); }

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& s, const Explorer::DeviceDocumentPlugin&,
      const score::DocumentContext&) override
  {
    return new FakeDevice{s};
  }
  Device::ProtocolSettingsWidget* makeSettingsWidget() override
  {
    return new FakeSettingsWidget;
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
  const Device::DeviceSettings& defaultSettings() const noexcept override
  {
    static const Device::DeviceSettings s = [] {
      Device::DeviceSettings set;
      set.name = QStringLiteral("fake");
      set.protocol = static_concreteKey();
      return set;
    }();
    return s;
  }
  void serializeProtocolSpecificSettings(
      const QVariant&, const VisitorVariant&) const override
  {
  }
  QVariant makeProtocolSpecificSettings(const VisitorVariant&) const override
  {
    return {};
  }
  bool checkCompatibility(const Device::DeviceSettings&, const Device::DeviceSettings&)
      const noexcept override
  {
    return true;
  }
};

Device::DeviceSettings fakeSettings(QString name, QStringList remote)
{
  Device::DeviceSettings s;
  s.name = std::move(name);
  s.protocol = FakeFactory::static_concreteKey();
  s.deviceSpecificSettings = QVariant::fromValue(std::move(remote));
  return s;
}

Device::Node leaf(const QString& name)
{
  Device::AddressSettings as;
  as.name = name;
  as.value = ossia::value{0};
  as.ioType = ossia::access_mode::BI;
  return Device::Node{as, nullptr};
}

struct Fixture
{
  score::Document* doc{};
  Explorer::DeviceDocumentPlugin* plug{};

  explicit Fixture(const score::GUIApplicationContext& ctx)
  {
    g_opts = {};

    // Register the fake protocol once per process
    auto& list = const_cast<Device::ProtocolFactoryList&>(
        ctx.interfaces<Device::ProtocolFactoryList>());
    if(!list.get(FakeFactory::static_concreteKey()))
      list.insert(std::make_unique<FakeFactory>());

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

    // Edit towards a host that answers nothing
    f.editDevice("cam", fakeSettings("cam", {}));
    spin(20);
    CHECK(f.device("cam")->connected());
    CHECK(f.tree("cam") == Names{"pan"});
  });
}
