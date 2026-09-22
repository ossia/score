// Score.enumerateDevices(protocols): enumerating is expensive -- it opens every
// /dev/video*, walks USB for GPhoto2, starts a BLE scan and NDI network
// discovery -- so asking for a subset of the protocols has to skip the
// factories it was not asked for rather than enumerate everything and filter
// the results afterwards. Two probe protocols count how many times their
// getEnumerators() is called.
//
// The filter is a protocol's user-visible name -- what the device dialog shows
// -- or its factory UUID, as one string or an array of them. The no-argument
// form still enumerates everything.

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <JS/Qml/EditContext.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QJSEngine>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

namespace
{
constexpr auto kProbeAUuid = "1a2b3c4d-0001-4a5b-9c8d-7e6f5a4b3c21";
constexpr auto kProbeBUuid = "1a2b3c4d-0002-4a5b-9c8d-7e6f5a4b3c21";

int g_enumeratedA{};
int g_enumeratedB{};

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
    return {{QStringLiteral("Probes"),
             new ProbeEnumerator{static_cast<const Self*>(this)->prettyName() + " 1"}}};
  }

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
  SCORE_CONCRETE("1a2b3c4d-0001-4a5b-9c8d-7e6f5a4b3c21")
public:
  static int& counter() noexcept { return g_enumeratedA; }
  QString prettyName() const noexcept override { return QStringLiteral("Probe A"); }
};

class ProbeBFactory final : public ProbeFactory<ProbeBFactory>
{
  SCORE_CONCRETE("1a2b3c4d-0002-4a5b-9c8d-7e6f5a4b3c21")
public:
  static int& counter() noexcept { return g_enumeratedB; }
  QString prettyName() const noexcept override { return QStringLiteral("Probe B"); }
};

void registerProbes(const score::GUIApplicationContext& ctx)
{
  auto& list
      = const_cast<Device::ProtocolFactoryList&>(ctx.interfaces<Device::ProtocolFactoryList>());
  if(!list.get(ProbeAFactory::static_concreteKey()))
    list.insert(std::make_unique<ProbeAFactory>());
  if(!list.get(ProbeBFactory::static_concreteKey()))
    list.insert(std::make_unique<ProbeBFactory>());
}

//! The console panel's engine: JS::EditJsContext bound to `Score`.
struct Console
{
  QJSEngine engine;
  JS::EditJsContext* api{};

  Console()
      : api{new JS::EditJsContext}
  {
    engine.globalObject().setProperty("Score", engine.newQObject(api));
  }
  ~Console() { delete api; }

  QJSValue eval(const QString& js)
  {
    auto res = engine.evaluate(js);
    INFO(res.toString().toStdString());
    REQUIRE(!res.isError());
    return res;
  }
};

//! The names Score.enumerateDevices(arg) reports, with the probe counters
//! reset first so that what each factory was asked for is readable after.
QStringList enumerated(Console& c, const QString& arg)
{
  g_enumeratedA = 0;
  g_enumeratedB = 0;

  const auto js = QStringLiteral(R"_(
    (function() {
      var e = Score.enumerateDevices(%1);
      e.enumerate = true;
      var out = [];
      for(var i = 0; i < e.devices.length; i++)
        out.push(e.devices[i].name);
      return out.join("|");
    })()
  )_").arg(arg);

  const auto res = c.eval(js).toString();
  return res.isEmpty() ? QStringList{} : res.split('|');
}
}

TEST_CASE(
    "Score.enumerateDevices does not run the protocols it was not asked for",
    "[integration][js][device][enumerate]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    registerProbes(ctx);
    REQUIRE(score::test::new_document(ctx) != nullptr);

    Console c;

    {
      INFO("one protocol, by name");
      CHECK(enumerated(c, "\"Probe A\"") == QStringList{"Probe A 1"});
      CHECK(g_enumeratedA == 1);
      CHECK(g_enumeratedB == 0);
    }

    {
      INFO("one protocol, by factory uuid");
      CHECK(
          enumerated(c, QStringLiteral("\"%1\"").arg(kProbeBUuid))
          == QStringList{"Probe B 1"});
      CHECK(g_enumeratedA == 0);
      CHECK(g_enumeratedB == 1);
    }

    {
      INFO("several protocols");
      const auto found = enumerated(c, "[\"Probe B\", \"Probe A\"]");
      CHECK(found.size() == 2);
      CHECK(found.contains("Probe A 1"));
      CHECK(found.contains("Probe B 1"));
      CHECK(g_enumeratedA == 1);
      CHECK(g_enumeratedB == 1);
    }

    {
      INFO("the name is matched case-insensitively");
      CHECK(enumerated(c, "[\"probe a\"]") == QStringList{"Probe A 1"});
      CHECK(g_enumeratedB == 0);
    }

    {
      INFO("a protocol no factory answers to enumerates nothing at all");
      CHECK(enumerated(c, "[\"Probe C\"]").isEmpty());
      CHECK(g_enumeratedA == 0);
      CHECK(g_enumeratedB == 0);
    }

    {
      INFO("no argument still enumerates everything");
      const auto found = enumerated(c, "");
      CHECK(found.contains("Probe A 1"));
      CHECK(found.contains("Probe B 1"));
      CHECK(g_enumeratedA == 1);
      CHECK(g_enumeratedB == 1);
    }

    {
      INFO("the deviceTypes property filters an existing enumerator");
      g_enumeratedA = 0;
      g_enumeratedB = 0;
      const auto res = c.eval(QStringLiteral(R"_(
        (function() {
          var e = Score.enumerateDevices();
          e.deviceTypes = ["Probe A"];
          e.enumerate = true;
          var out = [];
          for(var i = 0; i < e.devices.length; i++)
            out.push(e.devices[i].name);
          return out.join("|");
        })()
      )_")).toString();
      CHECK(res == QStringLiteral("Probe A 1"));
      CHECK(g_enumeratedA == 1);
      CHECK(g_enumeratedB == 0);
    }
  });
}
