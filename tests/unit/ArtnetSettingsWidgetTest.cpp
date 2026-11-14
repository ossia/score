// The DMX device's settings: the universes each transport lets one pick, the
// addresses the LED dialog places strips at, and the fixtures a device cannot output.

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/Artnet/ArtnetProtocolFactory.hpp>
#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>

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

QPushButton* button(QWidget& w, const QString& text)
{
  for(auto* b : w.findChildren<QPushButton*>())
    if(b->text() == text)
      return b;
  FAIL("no button " << text.toStdString());
  return nullptr;
}

//! Clicks the button and runs `f` on the modal dialog it opens, then accepts it.
//! `f` runs inside the dialog's event loop, so it must not throw: CHECK, not REQUIRE.
template <typename F>
void withDialog(QPushButton& b, F f)
{
  bool seen = false;
  QTimer::singleShot(0, [&] {
    auto* dial = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if(!dial)
      return;
    seen = true;
    f(*dial);
    dial->accept();
  });
  b.click();
  CHECK(seen);
}

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

TEST_CASE("An LED address stays within the device's universes", "[dmx][gui][led]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto w = makeWidget(ctx);
    Controls c{*w};
    c.transport->setCurrentIndex(ArtNet);
    c.universe->setValue(32766);
    c.count->setValue(2);

    withDialog(*button(*w, "Add LED strip"), [](QDialog& d) {
      auto* address = d.findChild<QSpinBox*>("address");
      auto* universe = d.findChild<QSpinBox*>("universe");
      auto* pixels = d.findChild<QSpinBox*>("pixels");
      CHECK(address);
      CHECK(universe);
      CHECK(pixels);
      if(!address || !universe || !pixels)
        return;
      CHECK(universe->minimum() == 32766);
      CHECK(universe->maximum() == 32767);
      CHECK(universe->value() == 32766);

      // 16 RGB pixels: 48 channels, which have to fit before the end of universe 32767.
      pixels->setValue(16);
      CHECK(address->maximum() == 2 * 512 - 48 + 1);
      address->setValue(999999);
      CHECK(address->value() == 2 * 512 - 48 + 1);

      universe->setValue(32767);
      CHECK(address->maximum() == 512 - 48 + 1);
      pixels->setValue(10);
      CHECK(address->maximum() == 512 - 30 + 1);
      pixels->setValue(16);
      address->setValue(999999);
    });

    const auto s = specific(*w);
    REQUIRE(s.fixtures.size() == 1);
    CHECK(s.fixtures[0].universe == 32767);
    CHECK(s.fixtures[0].address == 512 - 48);
  });
}

TEST_CASE("A DMX USB Pro outputs one universe and a Pro Mk2 two", "[dmx][gui][dmxusbpro]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto w = makeWidget(ctx);
    Controls c{*w};

    c.transport->setCurrentIndex(DMXUSBProMk2);
    CHECK(c.universe->minimum() == 0);
    CHECK(c.universe->maximum() == 1);
    CHECK(c.count->maximum() == 2);
    c.universe->setValue(1);
    CHECK(c.count->maximum() == 1);

    c.transport->setCurrentIndex(DMXUSBPro);
    CHECK(c.universe->maximum() == 0);
    CHECK(c.count->maximum() == 1);
  });
}

TEST_CASE("Fixtures in a universe the device cannot output are pointed out", "[dmx][gui][dmxusbpro]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto w = makeWidget(ctx);
    auto* warning = w->findChild<QLabel*>("universeWarning");
    REQUIRE(warning);

    Protocols::Artnet::Fixture first;
    first.fixtureName = "First";
    first.mode.channelNames = {"Dimmer"};
    first.universe = 0;
    Protocols::Artnet::Fixture second = first;
    second.fixtureName = "Second";
    second.universe = 1;

    Device::DeviceSettings set;
    set.name = "DMX";
    set.protocol = Protocols::ArtnetProtocolFactory::static_concreteKey();
    Protocols::ArtnetSpecificSettings in;
    in.fixtures = {first, second};
    in.universe_count = 2;

    in.transport = Protocols::ArtnetSpecificSettings::DMXUSBPRO_Mk2;
    set.deviceSpecificSettings = QVariant::fromValue(in);
    w->setSettings(set);
    CHECK(warning->isHidden());

    in.transport = Protocols::ArtnetSpecificSettings::DMXUSBPRO;
    set.deviceSpecificSettings = QVariant::fromValue(in);
    w->setSettings(set);
    CHECK(!warning->isHidden());
    CHECK(warning->text().contains("Second"));
    CHECK(!warning->text().contains("First"));

    Controls c{*w};
    c.transport->setCurrentIndex(DMXUSBProMk2);
    CHECK(warning->isHidden());
  });
}
