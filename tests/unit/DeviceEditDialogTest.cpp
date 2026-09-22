// Regression test for the use-after-free that crashed the "Add device" dialog
// when the protocol changed with an enumerator signal still in flight.
//
// The dialog's addItem/rmItem/sort lambdas capture a QTreeWidgetItem* owned by
// the tree, and switching protocol deletes it. An enumerator emitting from a
// worker thread makes the connection queued, and Qt drops posted metacalls only
// when their *receiver* dies - not when the sender does - so with the dialog as
// receiver the call still arrived, on a freed item.
//
// Only the deviceRemoved case reports cleanly under ASan: the other two
// dereference the item inside uninstrumented Qt.

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/Widgets/DeviceEditDialog.hpp>

#include <score/plugins/Interface.hpp>

#include <QApplication>
#include <QKeyEvent>
#include <QTreeWidget>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <chrono>
#include <thread>

namespace
{
// An enumerator whose signals are emitted from a plain std::thread, exactly
// like SimpleBLE's scan callback does in BLEEnumerator: Qt::AutoConnection then
// resolves to a queued connection and posts a metacall to the connection's
// context object.
class ThreadedEnumerator final : public Device::DeviceEnumerator
{
public:
  void enumerate(
      std::function<void(const QString&, const Device::DeviceSettings&)>) const override
  {
  }

  // Returns once the metacall is queued: emit() posts the event synchronously.
  void postAddFromWorkerThread(Device::DeviceSettings s)
  {
    std::thread t{[this, s = std::move(s)] { deviceAdded(s.name, s); }};
    t.join();
  }

  void postRemoveFromWorkerThread(QString name)
  {
    std::thread t{[this, name = std::move(name)] { deviceRemoved(name); }};
    t.join();
  }

  void postSortFromWorkerThread()
  {
    std::thread t{[this] { sort(); }};
    t.join();
  }
};

class DummySettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  Device::DeviceSettings m_settings;
  Device::DeviceSettings getSettings() const override { return m_settings; }
  void setSettings(const Device::DeviceSettings& s) override { m_settings = s; }
};

template <typename Self>
class DummyFactory : public Device::ProtocolFactory
{
public:
  QString category() const noexcept override { return QStringLiteral("Test"); }

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings&, const Explorer::DeviceDocumentPlugin&,
      const score::DocumentContext&) override
  {
    return nullptr;
  }
  Device::ProtocolSettingsWidget* makeSettingsWidget() override
  {
    return new DummySettingsWidget;
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
      set.name = QStringLiteral("dummy");
      set.protocol = Self::static_concreteKey();
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

// Protocol whose enumerator emits off-thread.
class AsyncFactory final : public DummyFactory<AsyncFactory>
{
  SCORE_CONCRETE("15cbe4b4-6c9c-4a70-9e2c-3a2c85dd53b0")
public:
  mutable ThreadedEnumerator* last{};

  QString prettyName() const noexcept override { return QStringLiteral("AAsync"); }
  Device::DeviceEnumerators getEnumerators(const score::DocumentContext&) const override
  {
    auto e = new ThreadedEnumerator;
    last = e;
    return {{QStringLiteral("Devices"), e}};
  }
};

// Protocol with no enumerator at all: selecting it is what clears the tree,
// which is precisely the case of the CAN protocol in the original report.
class PlainFactory final : public DummyFactory<PlainFactory>
{
  SCORE_CONCRETE("2f5df6f0-6e26-4a06-9f19-96b6d2b1f9f3")
public:
  QString prettyName() const noexcept override { return QStringLiteral("BPlain"); }
};

struct Harness
{
  Device::ProtocolFactoryList protocols;
  AsyncFactory* async{};
  PlainFactory* plain{};

  Harness()
  {
    auto a = std::make_unique<AsyncFactory>();
    async = a.get();
    protocols.insert(std::move(a));

    auto p = std::make_unique<PlainFactory>();
    plain = p.get();
    protocols.insert(std::move(p));
  }
};

// Build the dialog, select the async protocol, and hand back the enumerator it
// created. `settings` selection goes through the public setSettings(), which is
// the same code path a click on the protocol list takes.
struct Fixture
{
  Harness h;
  Explorer::DeviceEditDialog* dialog{};
  ThreadedEnumerator* enumerator{};

  Fixture(const score::GUIApplicationContext& ctx, score::Document& doc)
  {
    auto& model = Explorer::deviceExplorerFromContext(doc.context());
    dialog = new Explorer::DeviceEditDialog{
        model, h.protocols, Explorer::DeviceEditDialog::Creating, nullptr};

    dialog->setSettings(h.async->defaultSettings());
    QApplication::processEvents();

    enumerator = h.async->last;
    REQUIRE(enumerator != nullptr);
  }

  ~Fixture() { delete dialog; }

  Device::DeviceSettings someDevice(QString name) const
  {
    Device::DeviceSettings s;
    s.name = std::move(name);
    s.protocol = AsyncFactory::static_concreteKey();
    return s;
  }

  // Switch to the enumerator-less protocol. This is the m_devices->clear() that
  // frees the QTreeWidgetItem the pending metacall captured.
  void switchToPlain() { dialog->setSettings(h.plain->defaultSettings()); }
};
}

TEST_CASE("deviceAdded queued across a protocol switch is dropped", "[deviceexplorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    Fixture f{ctx, *doc};

    // Sanity: a synchronous add lands in the tree, so the test is exercising a
    // wiring that actually works.
    f.enumerator->deviceAdded(QStringLiteral("sync"), f.someDevice("sync"));

    // Now queue one from a worker thread and switch protocol before the event
    // loop ever runs.
    f.enumerator->postAddFromWorkerThread(f.someDevice("async"));
    f.switchToPlain();

    // Before the fix: heap-use-after-free in addItem -> QTreeWidgetItem::setExpanded.
    QApplication::processEvents();
    QApplication::processEvents();

    SUCCEED("no use-after-free while delivering the queued deviceAdded");
  });
}

TEST_CASE("deviceRemoved queued across a protocol switch is dropped", "[deviceexplorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    Fixture f{ctx, *doc};
    f.enumerator->deviceAdded(QStringLiteral("sync"), f.someDevice("sync"));

    f.enumerator->postRemoveFromWorkerThread(QStringLiteral("sync"));
    f.switchToPlain();

    QApplication::processEvents();
    QApplication::processEvents();

    SUCCEED("no use-after-free while delivering the queued deviceRemoved");
  });
}

TEST_CASE("sort queued across a protocol switch is dropped", "[deviceexplorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    Fixture f{ctx, *doc};
    f.enumerator->deviceAdded(QStringLiteral("sync"), f.someDevice("sync"));

    f.enumerator->postSortFromWorkerThread();
    f.switchToPlain();

    QApplication::processEvents();
    QApplication::processEvents();

    SUCCEED("no use-after-free while delivering the queued sort");
  });
}

// The dialog itself dying with events in flight must be safe too: here the
// receiver is destroyed, which Qt already handles, but the test pins the
// behaviour so a future refactor cannot regress it.
TEST_CASE("queued enumerator signals survive dialog destruction", "[deviceexplorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    {
      Fixture f{ctx, *doc};
      f.enumerator->postAddFromWorkerThread(f.someDevice("async"));
    }

    QApplication::processEvents();
    QApplication::processEvents();

    SUCCEED("no use-after-free after the dialog is gone");
  });
}

// Picking a device from the enumerated list ("Devices" column) fills the
// settings widget with that device's settings. When *editing* an existing
// device the score already refers to it by name, so the name must survive the
// pick; when *creating* one, taking the enumerated name is what the user wants.
namespace
{
struct ModeFixture
{
  Harness h;
  Explorer::DeviceEditDialog* dialog{};
  ThreadedEnumerator* enumerator{};

  ModeFixture(score::Document& doc, Explorer::DeviceEditDialog::Mode mode)
  {
    auto& model = Explorer::deviceExplorerFromContext(doc.context());
    dialog = new Explorer::DeviceEditDialog{model, h.protocols, mode, nullptr};

    auto s = h.async->defaultSettings();
    s.name = QStringLiteral("my camera");
    dialog->setSettings(s);
    dialog->show();
    QApplication::processEvents();

    enumerator = h.async->last;
    REQUIRE(enumerator != nullptr);
  }

  ~ModeFixture() { delete dialog; }

  //! Enumerate a device and click on it, as the user would.
  void pickEnumeratedDevice(const QString& name)
  {
    Device::DeviceSettings s;
    s.name = name;
    s.protocol = AsyncFactory::static_concreteKey();
    enumerator->deviceAdded(name, s);
    QApplication::processEvents();

    auto tree = dialog->devicesTree();
    REQUIRE(tree != nullptr);
    REQUIRE(tree->isVisible());
    QTreeWidgetItem* found{};
    for(int i = 0; i < tree->topLevelItemCount() && !found; i++)
    {
      auto cat = tree->topLevelItem(i);
      for(int j = 0; j < cat->childCount(); j++)
        if(cat->child(j)->text(0) == name)
        {
          found = cat->child(j);
          break;
        }
    }
    REQUIRE(found != nullptr);
    tree->setCurrentItem(found);
    QApplication::processEvents();
  }
};
}

TEST_CASE("editing a device and picking another enumerated device keeps its name", "[deviceexplorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    ModeFixture f{*doc, Explorer::DeviceEditDialog::Editing};
    REQUIRE(f.dialog->getSettings().name == "my camera");

    f.pickEnumeratedDevice(QStringLiteral("Logitech C920"));

    // The device-specific settings are taken from the pick, the name is not.
    CHECK(f.dialog->getSettings().name == "my camera");
  });
}

TEST_CASE("creating a device and picking an enumerated device takes its name", "[deviceexplorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    ModeFixture f{*doc, Explorer::DeviceEditDialog::Creating};
    f.pickEnumeratedDevice(QStringLiteral("Logitech C920"));

    CHECK(f.dialog->getSettings().name == "Logitech C920");
  });
}

// Navigating the protocol list with the arrow keys picks a protocol, exactly
// like clicking on it does: the settings widget below must follow the current
// item, not only the clicked one.
namespace
{
//! Rebuilding a protocol is debounced, as its enumerators probe hardware.
template <typename F>
bool waitFor(F&& f)
{
  for(int i = 0; i < 200 && !f(); i++)
  {
    QApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return f();
}

//! Spin long enough that a debounced rebuild would have happened.
void settle()
{
  for(int i = 0; i < 60; i++)
  {
    QApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

void pressKey(QWidget& w, Qt::Key key)
{
  QKeyEvent press{QEvent::KeyPress, key, Qt::NoModifier};
  QApplication::sendEvent(&w, &press);
  QKeyEvent release{QEvent::KeyRelease, key, Qt::NoModifier};
  QApplication::sendEvent(&w, &release);
}

QTreeWidgetItem* findItem(QTreeWidget& tree, const QString& name)
{
  for(int i = 0; i < tree.topLevelItemCount(); i++)
  {
    auto cat = tree.topLevelItem(i);
    for(int j = 0; j < cat->childCount(); j++)
      if(cat->child(j)->text(0) == name)
        return cat->child(j);
  }
  return nullptr;
}

struct KeyboardFixture
{
  Harness h;
  Explorer::DeviceEditDialog* dialog{};

  explicit KeyboardFixture(score::Document& doc)
  {
    auto& model = Explorer::deviceExplorerFromContext(doc.context());
    dialog = new Explorer::DeviceEditDialog{
        model, h.protocols, Explorer::DeviceEditDialog::Creating, nullptr};
    dialog->show();
    QApplication::processEvents();
  }

  ~KeyboardFixture() { delete dialog; }

  QTreeWidget& protocols() const
  {
    auto tree = dialog->protocolsTree();
    REQUIRE(tree != nullptr);
    return *tree;
  }

  void selectProtocol(const QString& name)
  {
    auto item = findItem(protocols(), name);
    REQUIRE(item != nullptr);
    protocols().setCurrentItem(item);
    REQUIRE(waitFor([&] { return dialog->getSettings().name == "dummy"; }));
  }
};
}

TEST_CASE("arrow keys change the selected protocol", "[deviceexplorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    KeyboardFixture f{*doc};
    f.selectProtocol(QStringLiteral("AAsync"));
    REQUIRE(f.dialog->getSettings().protocol == AsyncFactory::static_concreteKey());

    pressKey(f.protocols(), Qt::Key_Down);

    CHECK(waitFor([&] {
      return f.dialog->getSettings().protocol == PlainFactory::static_concreteKey();
    }));

    pressKey(f.protocols(), Qt::Key_Up);

    CHECK(waitFor([&] {
      return f.dialog->getSettings().protocol == AsyncFactory::static_concreteKey();
    }));
  });
}

// Category headers are enabled, so the arrow keys go through them: they carry
// no protocol and must leave the settings widget alone.
TEST_CASE("moving onto a category keeps the protocol settings", "[deviceexplorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    KeyboardFixture f{*doc};
    f.selectProtocol(QStringLiteral("AAsync"));

    pressKey(f.protocols(), Qt::Key_Up);
    REQUIRE(f.protocols().currentItem() == f.protocols().topLevelItem(0));

    settle();
    CHECK(f.dialog->getSettings().protocol == AsyncFactory::static_concreteKey());
  });
}

// Same story in the enumerated devices column.
TEST_CASE("arrow keys change the picked device", "[deviceexplorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    ModeFixture f{*doc, Explorer::DeviceEditDialog::Creating};
    f.pickEnumeratedDevice(QStringLiteral("Camera A"));

    Device::DeviceSettings other;
    other.name = QStringLiteral("Camera B");
    other.protocol = AsyncFactory::static_concreteKey();
    f.enumerator->deviceAdded(other.name, other);
    QApplication::processEvents();

    REQUIRE(f.dialog->getSettings().name == "Camera A");

    pressKey(*f.dialog->devicesTree(), Qt::Key_Down);
    QApplication::processEvents();

    CHECK(f.dialog->getSettings().name == "Camera B");
  });
}

// The reverse direction: setting the protocol from the outside (editing an
// existing device) moves the list's current item onto it.
TEST_CASE("setting the settings moves the protocol list", "[deviceexplorer]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    KeyboardFixture f{*doc};
    f.dialog->setSettings(f.h.plain->defaultSettings());
    QApplication::processEvents();

    auto current = f.protocols().currentItem();
    REQUIRE(current != nullptr);
    CHECK(current->text(0) == "BPlain");
    CHECK(f.dialog->getSettings().protocol == PlainFactory::static_concreteKey());
  });
}
