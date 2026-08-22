// The CAN protocol's device browser entry and its default interface.
//
// Both used to be missing or wrong: CANProtocolFactory had no getEnumerators()
// override, so selecting "CAN" in the add-device dialog showed an empty list,
// and defaultSettings() hardcoded "can0" -- an interface that does not exist on
// any machine without a physical CAN port, which turned every fresh CAN device
// into "no such CAN interface: can0: No such device" at connection time.
//
// Everything here is asserted against an independent walk of /sys/class/net
// done in the test itself, not against the implementation's own helper, so a
// bug in the helper cannot make the test agree with it.

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Protocols/CAN/CANSpecificSettings.hpp>

#include <core/document/Document.hpp>

#include <QDir>
#include <QFile>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_all.hpp>

#include <algorithm>

namespace
{
//! The uuid of Protocols::CANProtocolFactory. Spelled out rather than taken
//! from the class so that a renamed or moved factory still has to keep the key
//! its saved scores refer to.
constexpr UuidKey<Device::ProtocolFactory> can_protocol_key()
{
  return UuidKey<Device::ProtocolFactory>{"2492941c-18ee-4f96-ac3d-c3d42c0bb649"};
}

//! ARPHRD_CAN.
constexpr int arphrd_can = 280;

//! What the machine actually has, read straight out of sysfs.
QStringList sysfsCanInterfaces()
{
  QStringList out;
  QDir sys{"/sys/class/net"};
  for(const auto& name :
      sys.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System))
  {
    QFile type{"/sys/class/net/" + name + "/type"};
    if(!type.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;

    bool ok = false;
    if(type.readAll().trimmed().toInt(&ok) == arphrd_can && ok)
      out.push_back(name);
  }
  out.sort();
  return out;
}

Device::ProtocolFactory* canFactory(const score::GUIApplicationContext& ctx)
{
  auto& list = ctx.interfaces<Device::ProtocolFactoryList>();
  return list.get(can_protocol_key());
}
}

#if defined(__linux__)

TEST_CASE("the CAN protocol enumerates the interfaces sysfs reports", "[can]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* factory = canFactory(ctx);
    if(!factory)
    {
      SUCCEED("this build has no CAN protocol");
      return;
    }

    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto enumerators = factory->getEnumerators(doc->context());
    REQUIRE(enumerators.size() == 1);
    REQUIRE(enumerators[0].second != nullptr);

    std::vector<std::pair<QString, Device::DeviceSettings>> found;
    enumerators[0].second->enumerate(
        [&](const QString& name, const Device::DeviceSettings& s) {
      found.emplace_back(name, s);
    });

    const auto expected = sysfsCanInterfaces();

    QStringList names;
    for(auto& [n, s] : found)
      names.push_back(n);
    names.sort();
    REQUIRE(names == expected);

    // Each entry must be usable as-is: right protocol key, and an interface
    // name the kernel will accept. An entry whose settings still had to be
    // filled in by hand would defeat the point of the browser.
    for(auto& [n, s] : found)
    {
      REQUIRE(s.protocol == can_protocol_key());
      REQUIRE(!s.name.isEmpty());

      const auto specif = s.deviceSpecificSettings.value<Protocols::CANSpecificSettings>();
      REQUIRE(specif.interfaceName == n);
      REQUIRE(expected.contains(specif.interfaceName));
    }

    delete enumerators[0].second;
  });
}

TEST_CASE("the CAN protocol never enumerates a non-CAN netdev", "[can]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* factory = canFactory(ctx);
    if(!factory)
    {
      SUCCEED("this build has no CAN protocol");
      return;
    }

    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto enumerators = factory->getEnumerators(doc->context());
    REQUIRE(enumerators.size() == 1);

    QStringList names;
    enumerators[0].second->enumerate(
        [&](const QString& name, const Device::DeviceSettings&) {
      names.push_back(name);
    });

    // "lo" is on every machine and is ARPHRD_LOOPBACK (772), so it is the one
    // netdev we can always assert is filtered out. Matching on the name prefix
    // instead of the link type would also have let it through only by accident.
    REQUIRE(!names.contains("lo"));

    for(const auto& n : names)
    {
      QFile type{"/sys/class/net/" + n + "/type"};
      REQUIRE(type.open(QIODevice::ReadOnly | QIODevice::Text));
      REQUIRE(type.readAll().trimmed().toInt() == arphrd_can);
    }

    delete enumerators[0].second;
  });
}

TEST_CASE("the CAN default settings name an interface that exists", "[can]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* factory = canFactory(ctx);
    if(!factory)
    {
      SUCCEED("this build has no CAN protocol");
      return;
    }

    const auto settings = factory->defaultSettings();
    const auto specif
        = settings.deviceSpecificSettings.value<Protocols::CANSpecificSettings>();

    const auto present = sysfsCanInterfaces();

    if(present.isEmpty())
    {
      // Empty, not "can0": naming an interface that is not there produces a
      // failure at connect time for a value the user never chose.
      REQUIRE(specif.interfaceName.isEmpty());
    }
    else
    {
      REQUIRE(present.contains(specif.interfaceName));

      // A physical adapter wins over a vcan left over from a test.
      const bool anyPhysical = std::any_of(
          present.begin(), present.end(),
          [](const QString& n) { return !n.startsWith("vcan"); });
      if(anyPhysical)
        REQUIRE(!specif.interfaceName.startsWith("vcan"));
    }
  });
}

TEST_CASE("the CAN default settings follow the machine, not a cached value", "[can]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* factory = canFactory(ctx);
    if(!factory)
    {
      SUCCEED("this build has no CAN protocol");
      return;
    }

    // defaultSettings() returns a reference; two calls must still both describe
    // the machine as it is now (an adapter plugged in after startup has to show
    // up), and the second must not invalidate what the first said.
    const auto a = factory->defaultSettings();
    const auto b = factory->defaultSettings();

    REQUIRE(
        a.deviceSpecificSettings.value<Protocols::CANSpecificSettings>().interfaceName
        == b.deviceSpecificSettings.value<Protocols::CANSpecificSettings>()
               .interfaceName);
  });
}

#endif
