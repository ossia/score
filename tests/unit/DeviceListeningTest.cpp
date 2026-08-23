// The device explorer listens to exactly what it shows: the children of every
// unfolded node, and nothing else - across the device's tree being re-explored
// (which swaps its nodes in place) and its settings being edited.

#include "FakeDeviceProtocol.hpp"

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Update/UpdateDeviceSettings.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/DeviceExplorerView.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <QApplication>
#include <QElapsedTimer>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <set>
#include <string>

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

std::set<std::string> listened(Device::DeviceInterface& dev)
{
  std::set<std::string> out;
  for(const auto& a : dev.listening())
    out.insert(a.path.join('/').toStdString());
  return out;
}

using Names = std::set<std::string>;

struct Fixture
{
  score::Document* doc{};
  Explorer::DeviceDocumentPlugin* plug{};
  Explorer::DeviceExplorerWidget* widget{};

  explicit Fixture(const score::GUIApplicationContext& ctx)
  {
    g_opts = {};
    registerFakeProtocol(ctx);

    doc = score::test::new_document(ctx);
    REQUIRE(doc);
    plug = &doc->context().plugin<Explorer::DeviceDocumentPlugin>();

    widget = Explorer::findDeviceExplorerWidgetInstance(ctx);
    REQUIRE(widget);
    spin(20);
  }

  void submit(score::Command* cmd)
  {
    CommandDispatcher<>{doc->context().commandStack}.submit(cmd);
  }

  Device::Node* deviceNode(const QString& name) const
  {
    for(auto& n : plug->rootNode())
      if(n.get<Device::DeviceSettings>().name == name)
        return &n;
    return nullptr;
  }

  QModelIndex viewIndex(Device::Node& n)
  {
    return widget->proxyIndex(plug->explorer().modelIndexFromNode(n, 0));
  }

  void setExpanded(Device::Node& n, bool b)
  {
    widget->view()->setExpanded(viewIndex(n), b);
    spin(10);
  }
  bool isExpanded(Device::Node& n) { return widget->view()->isExpanded(viewIndex(n)); }

  Device::Node* child(Device::Node& parent, const QString& name)
  {
    for(auto& c : parent)
      if(c.displayName() == name)
        return &c;
    return nullptr;
  }
};
}

TEST_CASE("Unfolding a device listens to its children and folding stops", "[deviceexplorer][listening][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    f.submit(new Explorer::Command::LoadDevice{*f.plug, fakeSettings("cam", {"pan", "tilt"})});
    spin(20);

    auto dev = f.plug->list().findDevice("cam");
    REQUIRE(dev);
    auto node = f.deviceNode("cam");
    REQUIRE(node);

    CHECK(listened(*dev).empty());
    f.setExpanded(*node, true);
    CHECK(listened(*dev) == Names{"pan", "tilt"});
    f.setExpanded(*node, false);
    CHECK(listened(*dev).empty());
  });
}

TEST_CASE("Re-exploring a device keeps it unfolded and listened to", "[deviceexplorer][listening][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    f.submit(new Explorer::Command::LoadDevice{*f.plug, fakeSettings("cam", {"pan", "tilt"})});
    spin(20);
    auto dev = f.plug->list().findDevice("cam");
    auto node = f.deviceNode("cam");
    REQUIRE(dev);
    REQUIRE(node);

    f.setExpanded(*node, true);
    REQUIRE(listened(*dev) == Names{"pan", "tilt"});

    // A remote namespace: what the host has replaces what was replayed
    g_opts.remoteAuthoritative = true;

    // Edit: the tree is re-explored and swapped in place
    f.submit(new Explorer::Command::UpdateDeviceSettings{
        *f.plug, "cam", fakeSettings("cam", {"pan", "zoom"})});
    spin(40);

    node = f.deviceNode("cam");
    REQUIRE(node);
    CHECK(f.isExpanded(*node));
    // Listening follows what is shown now: pan and zoom, not tilt
    CHECK(listened(*dev) == Names{"pan", "zoom"});

    // Folded device: editing must not start listening to anything
    f.setExpanded(*node, false);
    CHECK(listened(*dev).empty());
    f.submit(new Explorer::Command::UpdateDeviceSettings{
        *f.plug, "cam", fakeSettings("cam", {"pan", "focus"})});
    spin(40);
    CHECK(listened(*dev).empty());
    CHECK(!f.isExpanded(*f.deviceNode("cam")));
  });
}

TEST_CASE("Nodes that leave the tree are no longer listened to", "[deviceexplorer][listening][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    f.submit(new Explorer::Command::LoadDevice{*f.plug, fakeSettings("cam", {"pan", "tilt"})});
    spin(20);
    auto dev = f.plug->list().findDevice("cam");
    auto node = f.deviceNode("cam");
    REQUIRE(dev);
    REQUIRE(node);
    f.setExpanded(*node, true);
    REQUIRE(dev->listeningRequests().size() == 2);

    // The device's tree is replaced by one without "tilt": the request for it
    // goes away with the row, it is not brought back by a later rebuild.
    Device::Node replacement{fakeSettings("cam", {"pan"}), nullptr};
    Device::AddressSettings as;
    as.name = "pan";
    as.value = ossia::value{0};
    as.ioType = ossia::access_mode::BI;
    replacement.push_back(Device::Node{as, nullptr});
    f.plug->explorer().replaceDevice(std::move(replacement));
    spin(20);

    CHECK(listened(*dev) == Names{"pan"});
    CHECK(dev->listeningRequests().size() == 1);
  });
}
