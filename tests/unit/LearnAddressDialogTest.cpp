// The Learn dialog of a port: the addresses whose values change are listed,
// and OK returns the one picked. The first one listed counts as picked, so that
// a single address can be learned without clicking it. While it is open every
// address of every device is listened to, not only those the explorer shows.

#include <Device/Node/DeviceNode.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Remove.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Process/Dataflow/AddressAccessorEditWidget.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/base/node_functions.hpp>

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include "FakeDeviceProtocol.hpp"

#include <algorithm>
#include <functional>

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
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Explorer::Command::LoadDevice{
            *plug, Device::Node{fakeSettings("dev", {"a", "b", "c", "grp/x"}), nullptr}});
  }

  //! What a device does when one of its values changes
  void receive(const QString& name, int v)
  {
    const State::Address addr{"dev", {name}};
    auto node = Device::try_getNodeFromAddress(plug->rootNode(), addr);
    REQUIRE(node);
    plug->explorer().updateValue(node, State::AddressAccessor{addr}, ossia::value{v});
  }

  Device::DeviceInterface* device() const { return plug->list().findDevice("dev"); }

  //! A value that reaches the device: only the addresses it listens to get
  //! it to the explorer
  void push(const QString& path, int v)
  {
    auto dev = device();
    REQUIRE(dev);
    REQUIRE(dev->getDevice());
    auto node
        = ossia::net::find_node(dev->getDevice()->get_root_node(), path.toStdString());
    REQUIRE(node);
    REQUIRE(node->get_parameter());
    node->get_parameter()->push_value(ossia::value{v});
    QCoreApplication::sendPostedEvents();
  }

  void removeDevice()
  {
    auto& root = plug->rootNode();
    auto it = std::find_if(root.begin(), root.end(), [](const Device::Node& n) {
      return n.displayName() == "dev";
    });
    REQUIRE(it != root.end());
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Explorer::Command::Remove{*plug, *it});
    REQUIRE(!device());
  }

  //! Runs the dialog; `user` acts on it once it is open, then OK is pressed
  std::optional<Device::FullAddressAccessorSettings>
  learn(std::function<void(QListWidget&)> user)
  {
    bool ran = false;
    QTimer::singleShot(0, [&] {
      auto dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      REQUIRE(dialog);
      auto list = dialog->findChild<QListWidget*>();
      REQUIRE(list);
      user(*list);
      dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
      ran = true;
    });
    auto res = Process::learnAddress(doc->context(), nullptr);
    REQUIRE(ran);
    return res;
  }
};

QString learned(const std::optional<Device::FullAddressAccessorSettings>& s)
{
  return s ? s->address.address.toString() : QString{};
}

QStringList sorted(const std::vector<State::Address>& addrs)
{
  QStringList res;
  for(const auto& a : addrs)
    res.push_back(a.toString());
  res.sort();
  return res;
}
}

TEST_CASE("Learn: the only address listed is learned without a click", "[learn]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto res = f.learn([&](QListWidget& list) {
      f.receive("b", 1);
      f.receive("b", 2);
      REQUIRE(list.count() == 1);
      CHECK(list.item(0)->isSelected());
    });
    CHECK(learned(res) == "dev:/b");
  });
}

TEST_CASE("Learn: the first address stays picked as others arrive", "[learn]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto res = f.learn([&](QListWidget& list) {
      f.receive("b", 1);
      // Listed in order, before the one picked
      f.receive("a", 1);
      f.receive("c", 1);
      REQUIRE(list.count() == 3);
      CHECK(list.currentItem() == list.item(1));
      CHECK(list.selectedItems() == QList<QListWidgetItem*>{list.item(1)});
    });
    CHECK(learned(res) == "dev:/b");
  });
}

TEST_CASE("Learn: another address can be picked", "[learn]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto res = f.learn([&](QListWidget& list) {
      f.receive("a", 1);
      f.receive("c", 1);
      list.setCurrentRow(1);
    });
    CHECK(learned(res) == "dev:/c");
  });
}

TEST_CASE("Learn: OK with nothing listed learns nothing", "[learn]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto res = f.learn([](QListWidget&) {});
    CHECK(!res);
  });
}

TEST_CASE("Learn: an address no row shows is learned", "[learn]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    const State::Address x{"dev", {"grp", "x"}};
    REQUIRE(f.device());
    REQUIRE(!sorted(f.device()->listening()).contains(x.toString()));

    auto res = f.learn([&](QListWidget& list) {
      // What the addresses held when the dialog opened is not listed
      QCoreApplication::sendPostedEvents();
      CHECK(list.count() == 0);
      f.push("grp/x", 3);
      REQUIRE(list.count() == 1);
      CHECK(list.item(0)->text().startsWith("dev:/grp/x"));
    });
    CHECK(learned(res) == "dev:/grp/x");
  });
}

TEST_CASE("Learn: once closed, devices listen to what they did before", "[learn]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto dev = f.device();
    REQUIRE(dev);
    // As the explorer does for a row it shows
    dev->setListening(State::Address{"dev", {"a"}}, true);
    const auto listening = sorted(dev->listening());
    const auto requests = sorted(dev->listeningRequests());
    REQUIRE(listening.contains("dev:/a"));
    REQUIRE(!listening.contains("dev:/grp/x"));

    f.learn([&](QListWidget&) {
      CHECK(sorted(dev->listening()).contains("dev:/grp/x"));
      CHECK(sorted(dev->listening()).contains("dev:/b"));
    });

    CHECK(sorted(dev->listening()) == listening);
    CHECK(sorted(dev->listeningRequests()) == requests);
  });
}

TEST_CASE("Learn: a device removed while the dialog is open", "[learn]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto res = f.learn([&](QListWidget& list) {
      f.push("grp/x", 3);
      REQUIRE(list.count() == 1);
      f.removeDevice();
      QCoreApplication::sendPostedEvents();
    });
    CHECK(!res);

    // The dialog can be opened again, with nothing left to listen to there
    res = f.learn([](QListWidget& list) { CHECK(list.count() == 0); });
    CHECK(!res);
  });
}
