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
#include <QTreeWidget>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_all.hpp>

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
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)>)
      const override
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
  void serializeProtocolSpecificSettings(const QVariant&, const VisitorVariant&)
      const override
  {
  }
  QVariant makeProtocolSpecificSettings(const VisitorVariant&) const override
  {
    return {};
  }
  bool checkCompatibility(
      const Device::DeviceSettings&, const Device::DeviceSettings&) const noexcept override
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
  Device::DeviceEnumerators
  getEnumerators(const score::DocumentContext&) const override
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
