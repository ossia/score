// A declarative `DeviceEnumerator { deviceType: ...; enumerate: true }` may
// only walk the protocol it names, in every order QML can write those two
// properties -- and QML chooses that order, not the author: literals land
// before script bindings, and two literals in reverse source order
// (qqmlirbuilder.cpp, QmlIR::Object::appendBinding prepends). An unfiltered
// walk opens every /dev/video*, walks USB and starts a BLE scan; on Windows it
// is a synchronous COM/RPC call on the STA main thread that never returns, so
// QQmlComponent::create() never does either. The second case times that shape
// with a protocol that blocks inside getEnumerators().

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QElapsedTimer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <chrono>
#include <thread>

namespace
{
int g_enumeratedA{};
int g_enumeratedB{};
int g_enumeratedSlow{};

//! Reports one device, synchronously, the way Camera and the MIDI backends do.
class ProbeEnumerator final : public Device::DeviceEnumerator
{
public:
  explicit ProbeEnumerator(QString name)
      : m_name{std::move(name)}
  {
  }

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    Device::DeviceSettings s;
    s.name = m_name;
    f(m_name, s);
  }

private:
  QString m_name;
};

class ProbeSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  Device::DeviceSettings getSettings() const override { return {}; }
  void setSettings(const Device::DeviceSettings&) override { }
};

template <typename Self>
class ProbeFactory : public Device::ProtocolFactory
{
public:
  QString category() const noexcept override { return QStringLiteral("Test"); }

  Device::DeviceEnumerators getEnumerators(const score::DocumentContext&) const override
  {
    ++Self::counter();
    Self::onEnumerated();
    return {{QStringLiteral("Probes"),
             new ProbeEnumerator{static_cast<const Self*>(this)->prettyName() + " 1"}}};
  }

  static void onEnumerated() { }

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings&, const Explorer::DeviceDocumentPlugin&,
      const score::DocumentContext&) override
  {
    return nullptr;
  }
  Device::ProtocolSettingsWidget* makeSettingsWidget() override
  {
    return new ProbeSettingsWidget;
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
    static const Device::DeviceSettings s{};
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
  bool checkCompatibility(const Device::DeviceSettings&, const Device::DeviceSettings&)
      const noexcept override
  {
    return true;
  }
};

class ProbeAFactory final : public ProbeFactory<ProbeAFactory>
{
  SCORE_CONCRETE("2b3c4d5e-0001-4a5b-9c8d-7e6f5a4b3c21")
public:
  static int& counter() noexcept { return g_enumeratedA; }
  QString prettyName() const noexcept override { return QStringLiteral("Probe A"); }
};

class ProbeBFactory final : public ProbeFactory<ProbeBFactory>
{
  SCORE_CONCRETE("2b3c4d5e-0002-4a5b-9c8d-7e6f5a4b3c21")
public:
  static int& counter() noexcept { return g_enumeratedB; }
  QString prettyName() const noexcept override { return QStringLiteral("Probe B"); }
};

//! The Windows failure in miniature: on that platform the wait is a COM/RPC
//! call that never returns, here a sleep long enough to show in the clock.
constexpr auto kSlowProbeDelay = std::chrono::milliseconds{3000};

class SlowProbeFactory final : public ProbeFactory<SlowProbeFactory>
{
  SCORE_CONCRETE("2b3c4d5e-0003-4a5b-9c8d-7e6f5a4b3c21")
public:
  static int& counter() noexcept { return g_enumeratedSlow; }
  static void onEnumerated() { std::this_thread::sleep_for(kSlowProbeDelay); }
  QString prettyName() const noexcept override { return QStringLiteral("Slow Probe"); }
};

template <typename F>
void registerProbe(const score::GUIApplicationContext& ctx)
{
  auto& list = const_cast<Device::ProtocolFactoryList&>(
      ctx.interfaces<Device::ProtocolFactoryList>());
  if(!list.get(F::static_concreteKey()))
    list.insert(std::make_unique<F>());
}

//! Instantiate `qml` and return its root object, which the caller owns.
QObject* instantiate(QQmlEngine& engine, const QByteArray& body)
{
  const auto qml = QByteArray{"import QtQml\nimport Score.UI as UI\n"} + body;
  QQmlComponent comp(&engine);
  comp.setData(qml, QUrl{});
  INFO(comp.errorString().toStdString());
  REQUIRE(comp.isReady());
  auto* obj = comp.create();
  INFO(comp.errorString().toStdString());
  REQUIRE(obj != nullptr);
  return obj;
}

//! The names the enumerator ended up reporting, read the way a binding would.
QString enumeratedNames(QObject* obj)
{
  QQmlExpression expr(
      qmlContext(obj), obj,
      QStringLiteral("(function() { var o = []; for(var i = 0; i < devices.length; i++)"
                     " o.push(devices[i].name); return o.join('|'); })()"));
  const auto v = expr.evaluate();
  INFO(expr.error().toString().toStdString());
  REQUIRE(!expr.hasError());
  return v.toString();
}
}

TEST_CASE(
    "a QML DeviceEnumerator walks only its deviceType, whatever the property order",
    "[integration][js][qml][device][enumerate]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    registerProbe<ProbeAFactory>(ctx);
    registerProbe<ProbeBFactory>(ctx);
    REQUIRE(score::test::new_document(ctx) != nullptr);

    QQmlEngine engine;
    // A context property makes `deviceType: wantedProtocol` an expression
    // rather than a constant, which is the shape that matters.
    engine.rootContext()->setContextProperty(
        QStringLiteral("wantedProtocol"), QStringLiteral("Probe A"));

    // The four orders a QML author can write. None of them may walk Probe B.
    const std::pair<const char*, QByteArray> shapes[] = {
        {"expression filter, then the constant",
         "UI.DeviceEnumerator { deviceType: wantedProtocol; enumerate: true }"},
        {"the constant, then the expression filter",
         "UI.DeviceEnumerator { enumerate: true; deviceType: wantedProtocol }"},
        {"two constants, filter written first",
         "UI.DeviceEnumerator { deviceType: \"Probe A\"; enumerate: true }"},
        {"two constants, enumerate written first",
         "UI.DeviceEnumerator { enumerate: true; deviceType: \"Probe A\" }"},
        // Two chained script bindings: finalize() may enable the filter while
        // the parent's own property is still empty, and it must still have
        // converged by componentComplete().
        {"nested in another object, filter from the parent",
         R"_(QtObject {
               id: root
               property string wanted: wantedProtocol
               property QtObject en: UI.DeviceEnumerator {
                 deviceType: root.wanted
                 enumerate: true
               }
             })_"},
    };

    for(const auto& [what, body] : shapes)
    {
      INFO(what);
      g_enumeratedA = 0;
      g_enumeratedB = 0;

      std::unique_ptr<QObject> root{instantiate(engine, body)};
      auto* en = root.get();
      if(const auto nested = root->property("en"); nested.isValid())
        en = nested.value<QObject*>();
      REQUIRE(en != nullptr);

      // Without the fix this is 1 for every shape but the fourth.
      CHECK(g_enumeratedB == 0);
      // And exactly one walk, not one unfiltered plus one filtered.
      CHECK(g_enumeratedA == 1);
      CHECK(enumeratedNames(en) == QStringLiteral("Probe A 1"));
    }

    {
      INFO("no filter still means every protocol");
      g_enumeratedA = 0;
      g_enumeratedB = 0;

      std::unique_ptr<QObject> root{
          instantiate(engine, "UI.DeviceEnumerator { enumerate: true }")};
      CHECK(g_enumeratedA == 1);
      CHECK(g_enumeratedB == 1);
      const auto names = enumeratedNames(root.get());
      CHECK(names.split('|').contains(QStringLiteral("Probe A 1")));
      CHECK(names.split('|').contains(QStringLiteral("Probe B 1")));
    }

    {
      INFO("an enumerator left disarmed walks nothing");
      g_enumeratedA = 0;
      g_enumeratedB = 0;

      std::unique_ptr<QObject> root{
          instantiate(engine, "UI.DeviceEnumerator { deviceType: wantedProtocol }")};
      CHECK(g_enumeratedA == 0);
      CHECK(g_enumeratedB == 0);
    }
  });
}

TEST_CASE(
    "a protocol a QML DeviceEnumerator did not ask for cannot block its creation",
    "[integration][js][qml][device][enumerate]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    registerProbe<ProbeAFactory>(ctx);
    registerProbe<SlowProbeFactory>(ctx);
    REQUIRE(score::test::new_document(ctx) != nullptr);

    QQmlEngine engine;

    g_enumeratedA = 0;
    g_enumeratedSlow = 0;

    QElapsedTimer t;
    t.start();
    std::unique_ptr<QObject> root{instantiate(
        engine, "UI.DeviceEnumerator { deviceType: \"Probe A\"; enumerate: true }")};
    const auto elapsed = t.elapsed();

    CHECK(g_enumeratedSlow == 0);
    CHECK(g_enumeratedA == 1);
    // Half the blocking probe's delay: anything near or above it means the
    // creation entered a protocol it was not asked for.
    CHECK(elapsed < kSlowProbeDelay.count() / 2);
    CHECK(enumeratedNames(root.get()) == QStringLiteral("Probe A 1"));

    {
      // ...and it is still reachable when asked for: a filter, not a refusal.
      INFO("the slow protocol is entered when it is the one asked for");
      g_enumeratedSlow = 0;
      std::unique_ptr<QObject> slow{instantiate(
          engine, "UI.DeviceEnumerator { deviceType: \"Slow Probe\"; enumerate: true }")};
      CHECK(g_enumeratedSlow == 1);
      CHECK(enumeratedNames(slow.get()) == QStringLiteral("Slow Probe 1"));
    }
  });
}
