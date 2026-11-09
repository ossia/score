// The Learn dialog of a port: the addresses whose values change are listed,
// and OK returns the one picked. The first one listed counts as picked, so that
// a single address can be learned without clicking it.

#include <Device/Node/DeviceNode.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Process/Dataflow/AddressAccessorEditWidget.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

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
            *plug, Device::Node{fakeSettings("dev", {"a", "b", "c"}), nullptr}});
  }

  //! What a device does when one of its values changes
  void receive(const QString& name, int v)
  {
    const State::Address addr{"dev", {name}};
    auto node = Device::try_getNodeFromAddress(plug->rootNode(), addr);
    REQUIRE(node);
    plug->explorer().updateValue(node, State::AddressAccessor{addr}, ossia::value{v});
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
