// The DMX device's settings: the universes each transport lets one pick.


#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/Artnet/ArtnetProtocolFactory.hpp>
#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QComboBox>
#include <QSpinBox>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>

#include <memory>

namespace
{
enum TransportIndex
{
  ArtNet = 0,
  E131 = 1,
  DMXUSBPro = 2,
  DMXUSBProMk2 = 3,
  OpenDMX = 4
};

std::unique_ptr<Device::ProtocolSettingsWidget>
makeWidget(const score::GUIApplicationContext& ctx)
{
  auto* factory = ctx.interfaces<Device::ProtocolFactoryList>().get(
      Protocols::ArtnetProtocolFactory::static_concreteKey());
  REQUIRE(factory);
  std::unique_ptr<Device::ProtocolSettingsWidget> w{factory->makeSettingsWidget()};
  REQUIRE(w);
  return w;
}

struct Controls
{
  QComboBox* transport{};
  QSpinBox* universe{};
  QSpinBox* count{};

  explicit Controls(QWidget& w)
      : transport{w.findChild<QComboBox*>("transport")}
      , universe{w.findChild<QSpinBox*>("universe")}
      , count{w.findChild<QSpinBox*>("universeCount")}
  {
    REQUIRE(transport);
    REQUIRE(universe);
    REQUIRE(count);
  }
};

Protocols::ArtnetSpecificSettings specific(const Device::ProtocolSettingsWidget& w)
{
  return w.getSettings().deviceSpecificSettings.value<Protocols::ArtnetSpecificSettings>();
}
}

TEST_CASE("The start universe follows the transport's numbering", "[dmx][gui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto w = makeWidget(ctx);
    Controls c{*w};

    c.transport->setCurrentIndex(E131);
    CHECK(c.universe->minimum() == 1);
    CHECK(c.universe->maximum() == 63999);

    c.transport->setCurrentIndex(ArtNet);
    CHECK(c.universe->minimum() == 0);
    CHECK(c.universe->maximum() == 32767);

    c.transport->setCurrentIndex(DMXUSBPro);
    CHECK(c.universe->minimum() == 0);
    CHECK(c.universe->maximum() == 0);
    CHECK(c.count->maximum() == 1);

    c.transport->setCurrentIndex(OpenDMX);
    CHECK(c.universe->maximum() == 0);
    CHECK(c.count->maximum() == 1);

    c.transport->setCurrentIndex(E131);
    c.universe->setValue(63999);
    CHECK(c.count->maximum() == 1);
    c.universe->setValue(63990);
    CHECK(c.count->maximum() == 10);
  });
}

TEST_CASE("The universe count stops at the last universe on the wire", "[dmx][gui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto w = makeWidget(ctx);
    Controls c{*w};

    c.transport->setCurrentIndex(ArtNet);
    c.universe->setValue(32767);
    CHECK(c.count->maximum() == 1);
    c.universe->setValue(32760);
    CHECK(c.count->maximum() == 8);
    c.count->setValue(8);

    auto s = specific(*w);
    CHECK(s.start_universe == 32760);
    CHECK(s.start_universe + s.universe_count - 1 == 32767);
  });
}

TEST_CASE("Settings past the transport's universes are shown within them", "[dmx][gui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto w = makeWidget(ctx);

    Device::DeviceSettings set;
    set.name = "DMX";
    set.protocol = Protocols::ArtnetProtocolFactory::static_concreteKey();
    Protocols::ArtnetSpecificSettings in;
    in.transport = Protocols::ArtnetSpecificSettings::ArtNetV2;
    in.start_universe = 300;
    in.universe_count = 4;
    set.deviceSpecificSettings = QVariant::fromValue(in);
    w->setSettings(set);

    auto out = specific(*w);
    CHECK(out.start_universe == 300);
    CHECK(out.universe_count == 4);

    in.transport = Protocols::ArtnetSpecificSettings::E131;
    in.start_universe = 0;
    set.deviceSpecificSettings = QVariant::fromValue(in);
    w->setSettings(set);
    out = specific(*w);
    CHECK(out.start_universe == 1);
  });
}
