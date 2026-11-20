// What a MIDI input makes of the controllers that only mean something across
// several messages: bank select (CC 0, 32), data entry (CC 6, 38) and RPN /
// NRPN selection (CC 98 to 101).
//
// A keyboard commonly sets its pitch bend range with CC 101 = 0, CC 100 = 0,
// CC 6 = range and no CC 38. Whatever the device is set to do with these
// controllers, everything it sends afterwards has to keep reaching the
// document.
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/MIDI/MIDIDevice.hpp>
#include <Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <Protocols/MIDI/MIDISpecificSettings.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/network/context.hpp>
#include <ossia/protocols/midi/midi_device.hpp>
#include <ossia/protocols/midi/midi_protocol.hpp>

#include <libremidi/configurations.hpp>
#include <libremidi/defaults.hpp>
#include <libremidi/detail/conversion.hpp>
#include <libremidi/message.hpp>

#include <QComboBox>
#include <QDataStream>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>

#include <memory>
#include <vector>

namespace
{
using libremidi::controller_translation;
using words = std::vector<uint32_t>;
using bytes = std::vector<uint8_t>;

struct midi_fixture
{
  ossia::net::network_context_ptr ctx
      = std::make_shared<ossia::net::network_context>();
  libremidi::input_configuration conf;
  ossia::net::midi::midi_device dev;

  explicit midi_fixture(ossia::net::midi::midi_protocol_configuration c)
      : dev{"midi", std::make_unique<ossia::net::midi::midi_protocol>(
                        ctx, std::move(c), conf,
                        libremidi::midi_in_configuration_for(libremidi::API::KEYBOARD))}
  {
    protocol().enable_registration();
  }

  ossia::net::midi::midi_protocol& protocol() const noexcept
  {
    return static_cast<ossia::net::midi::midi_protocol&>(dev.get_protocol());
  }

  void send(bytes b)
  {
    libremidi::message m;
    m.bytes.assign(b.begin(), b.end());
    conf.on_message(std::move(m));
  }

  // The words of the channel voice packets queued for the graph.
  words drain()
  {
    words out;
    libremidi::ump u;
    while(protocol().messages.try_dequeue(u))
    {
      out.push_back(u.data[0]);
      out.push_back(u.data[1]);
    }
    return out;
  }
};

ossia::net::midi::midi_protocol_configuration config(controller_translation c)
{
  ossia::net::midi::midi_protocol_configuration conf;
  conf.device_name = "midi";
  conf.controllers = c;
  return conf;
}

// The MIDI 1 bytes a MIDI 1 consumer of the graph (a plug-in, a MIDI output)
// gets from these packets.
bytes as_midi1(const words& ump)
{
  libremidi::midi2_to_midi1 conv;
  bytes out;
  conv.convert(ump.data(), ump.size(), 0, [&](const uint8_t* m, std::size_t n, int64_t) {
    out.insert(out.end(), m, m + n);
    return stdx::error{};
  });
  return out;
}

const words note_60{0x40903C00, 0x80000000};
const words note_62{0x40903E00, 0x80000000};

QByteArray saved(const Device::ProtocolFactory& factory, const Protocols::MIDISpecificSettings& s)
{
  JSONReader r;
  r.stream.StartObject();
  factory.serializeProtocolSpecificSettings(QVariant::fromValue(s), r.toVariant());
  r.stream.EndObject();
  return r.toByteArray();
}

Protocols::MIDISpecificSettings loaded(const Device::ProtocolFactory& factory, const QByteArray& text)
{
  rapidjson::Document doc;
  doc.Parse(text.constData(), text.size());
  REQUIRE(!doc.HasParseError());
  JSONWriter wrt{doc};
  return factory.makeProtocolSpecificSettings(wrt.toVariant())
      .value<Protocols::MIDISpecificSettings>();
}
}

TEST_CASE("A MIDI input forwards its controllers as they are unless told otherwise", "[midi][controllers]")
{
  CHECK(
      ossia::net::midi::midi_protocol_configuration{}.controllers
      == controller_translation::raw);
  CHECK(Protocols::MIDISpecificSettings{}.controllers == controller_translation::raw);

  // The two fields older code gives positionally still say what they said.
  const ossia::net::midi::midi_protocol_configuration positional{"name", true};
  CHECK(positional.velocity_zero_is_note_off);
  CHECK(positional.controllers == controller_translation::raw);
}

TEST_CASE(
    "A pitch bend range sent without CC 38 does not silence the input", "[midi][controllers]")
{
  SECTION("controllers forwarded as they are")
  {
    midi_fixture f{config(controller_translation::raw)};
    f.send({0x90, 0x3C, 0x40});
    CHECK(f.drain() == note_60);

    f.send({0xB0, 0x65, 0x00});
    f.send({0xB0, 0x64, 0x00});
    f.send({0xB0, 0x06, 0x0C});
    const auto controllers = f.drain();
    CHECK(
        controllers
        == words{0x40B06500, 0x00000000, 0x40B06400, 0x00000000, 0x40B00600, 0x18000000});
    CHECK(as_midi1(controllers) == bytes{0xB0, 0x65, 0x00, 0xB0, 0x64, 0x00, 0xB0, 0x06, 0x0C});

    for(int i = 0; i < 100; i++)
    {
      f.send({0x90, 0x3E, 0x40});
      REQUIRE(f.drain() == note_62);
    }
  }

  SECTION("controllers combined")
  {
    midi_fixture f{config(controller_translation::semantic)};
    f.send({0x90, 0x3C, 0x40});
    CHECK(f.drain() == note_60);

    f.send({0xB0, 0x65, 0x00});
    f.send({0xB0, 0x64, 0x00});
    CHECK(f.drain().empty());
    f.send({0xB0, 0x06, 0x0C});
    // Registered controller 0 (pitch bend sensitivity), 12 semitones.
    CHECK(f.drain() == words{0x40200000, 0x18000000});

    for(int i = 0; i < 100; i++)
    {
      f.send({0x90, 0x3E, 0x40});
      REQUIRE(f.drain() == note_62);
    }

    // A device that does send CC 38 refines the value.
    f.send({0xB0, 0x26, 0x40});
    CHECK(f.drain() == words{0x40200000, 0x19000000});
  }
}

TEST_CASE("Bank select on a MIDI input", "[midi][controllers]")
{
  SECTION("controllers forwarded as they are")
  {
    midi_fixture f{config(controller_translation::raw)};
    f.send({0xB0, 0x00, 0x01});
    f.send({0xB0, 0x20, 0x02});
    f.send({0xC0, 0x05});
    const auto got = f.drain();
    CHECK(
        got
        == words{0x40B00000, 0x02000000, 0x40B02000, 0x04000000, 0x40C00000, 0x05000000});
    CHECK(as_midi1(got) == bytes{0xB0, 0x00, 0x01, 0xB0, 0x20, 0x02, 0xC0, 0x05});
  }

  SECTION("controllers combined")
  {
    midi_fixture f{config(controller_translation::semantic)};
    f.send({0xB0, 0x00, 0x01});
    f.send({0xB0, 0x20, 0x02});
    CHECK(f.drain().empty());
    f.send({0xC0, 0x05});
    const auto got = f.drain();
    CHECK(got == words{0x40C00001, 0x05000102});
    CHECK(as_midi1(got) == bytes{0xB0, 0x00, 0x01, 0xB0, 0x20, 0x02, 0xC0, 0x05});
  }
}

TEST_CASE("A message that cannot be translated costs that message only", "[midi][controllers]")
{
  for(auto mode : {controller_translation::raw, controller_translation::semantic})
  {
    midi_fixture f{config(mode)};
    // Each several times: reporting them must not get in the way either.
    for(int i = 0; i < 50; i++)
    {
      f.send({0x90, 0x3C});
      f.send({0xF0, 0x01, 0x02});
      f.send({0xF4});
    }
    CHECK(f.drain().empty());
    f.send({0x90, 0x3C, 0x40});
    CHECK(f.drain() == note_60);
  }
}

TEST_CASE("The controllers setting of a MIDI input device", "[midi][controllers][gui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* factory = ctx.interfaces<Device::ProtocolFactoryList>().get(
        Protocols::MIDIInputProtocolFactory::static_concreteKey());
    REQUIRE(factory);

    // What released versions wrote: nothing about controllers.
    const auto old = loaded(*factory, R"_({
      "API": 2, "IO": 0, "Port": 3, "PortName": "Keyboard", "DisplayName": "Keyboard",
      "CreateWholeTree": true, "VirtualPort": false, "VelocityZeroIsNoteOff": true
    })_");
    CHECK(old.controllers == controller_translation::raw);
    CHECK(old.velocityZeroIsNoteOff);
    CHECK(old.handle.port_name == "Keyboard");

    // Older still: the port was an "Endpoint" name.
    const auto older = loaded(*factory, R"_({"IO": 0, "Endpoint": "Keyboard"})_");
    CHECK(older.controllers == controller_translation::raw);
    CHECK(older.handle.port_name == "Keyboard");

    // Out-of-range values are the default.
    CHECK(
        loaded(*factory, R"_({"IO": 0, "Port": 3, "ControllerTranslation": 7})_").controllers
        == controller_translation::raw);

    // Both choices come back from a save.
    for(auto mode : {controller_translation::raw, controller_translation::semantic})
    {
      Protocols::MIDISpecificSettings s;
      s.io = Protocols::MIDISpecificSettings::IO::In;
      s.handle.port_name = "Keyboard";
      s.controllers = mode;
      const auto back = loaded(*factory, saved(*factory, s));
      CHECK(back.controllers == mode);
      CHECK(back.handle.port_name == "Keyboard");

      // And reach the protocol a (re)connection builds.
      CHECK(Protocols::makeProtocolConfiguration(back).controllers == mode);
    }

    // The dialog shows the choice and gives it back.
    std::unique_ptr<Device::ProtocolSettingsWidget> widget{factory->makeSettingsWidget()};
    QComboBox* combo = widget->findChild<QComboBox*>();
    REQUIRE(combo);
    REQUIRE(combo->count() == 2);
    for(int i = 0; i < combo->count(); i++)
      CHECK(!combo->itemData(i, Qt::ToolTipRole).toString().isEmpty());

    Device::DeviceSettings dev;
    dev.name = "Keyboard";
    dev.protocol = Protocols::MIDIInputProtocolFactory::static_concreteKey();
    Protocols::MIDISpecificSettings s;
    s.io = Protocols::MIDISpecificSettings::IO::In;
    dev.deviceSpecificSettings = QVariant::fromValue(s);

    widget->setSettings(dev);
    CHECK(combo->currentIndex() == 0);
    CHECK(
        widget->getSettings().deviceSpecificSettings.value<Protocols::MIDISpecificSettings>().controllers
        == controller_translation::raw);

    int changes = 0;
    QObject::connect(
        widget.get(), &Device::ProtocolSettingsWidget::changed, widget.get(),
        [&] { changes++; });
    combo->setCurrentIndex(1);
    CHECK(changes == 1);
    CHECK(
        widget->getSettings().deviceSpecificSettings.value<Protocols::MIDISpecificSettings>().controllers
        == controller_translation::semantic);

    s.controllers = controller_translation::semantic;
    dev.deviceSpecificSettings = QVariant::fromValue(s);
    widget->setSettings(dev);
    CHECK(combo->currentIndex() == 1);
  });
}

namespace
{
QByteArray int32_bytes(int32_t v)
{
  QByteArray a;
  QDataStream d{&a, QIODevice::WriteOnly};
  d.setVersion(QDataStream::Qt_DefaultCompiledVersion);
  d << v;
  return a;
}
const QByteArray delimiter = int32_bytes(int32_t(0xDEADBEEF));

Protocols::MIDISpecificSettings detailedSettings()
{
  Protocols::MIDISpecificSettings s;
  s.io = Protocols::MIDISpecificSettings::IO::In;
  s.handle.api = libremidi::API::ALSA_SEQ;
  s.createWholeTree = true;
  s.virtualPort = true;
  s.handle.container = std::string{"container"};
  s.handle.device = libremidi::usb_device_identifier{0x1234, 0x5678};
  s.handle.port = 0x0102030405060708ULL;
  s.handle.manufacturer = "Maker";
  s.handle.product = "Keys 88";
  s.handle.serial = "SN-42";
  s.handle.device_name = "Keys 88 device";
  s.handle.port_name = "Keys 88 MIDI 1";
  s.handle.display_name = "Keys 88: MIDI 1";
  s.velocityZeroIsNoteOff = true;
  return s;
}

void checkSame(const Protocols::MIDISpecificSettings& a, const Protocols::MIDISpecificSettings& b)
{
  CHECK(a.io == b.io);
  CHECK(a.handle.api == b.handle.api);
  CHECK(a.createWholeTree == b.createWholeTree);
  CHECK(a.virtualPort == b.virtualPort);
  CHECK(a.handle.container == b.handle.container);
  CHECK(a.handle.device == b.handle.device);
  CHECK(a.handle.port == b.handle.port);
  CHECK(a.handle.manufacturer == b.handle.manufacturer);
  CHECK(a.handle.product == b.handle.product);
  CHECK(a.handle.serial == b.handle.serial);
  CHECK(a.handle.device_name == b.handle.device_name);
  CHECK(a.handle.port_name == b.handle.port_name);
  CHECK(a.handle.display_name == b.handle.display_name);
  CHECK(a.velocityZeroIsNoteOff == b.velocityZeroIsNoteOff);
}

}

TEST_CASE(
    "The controllers setting of a MIDI device goes through its binary form",
    "[midi][controllers][gui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* factory = ctx.interfaces<Device::ProtocolFactoryList>().get(
        Protocols::MIDIInputProtocolFactory::static_concreteKey());
    REQUIRE(factory);

    const auto expected = detailedSettings();

    for(auto mode : {controller_translation::raw, controller_translation::semantic})
    {
      auto s = expected;
      s.controllers = mode;
      Device::DeviceSettings dev;
      dev.name = "Keys";
      dev.protocol = Protocols::MIDIInputProtocolFactory::static_concreteKey();
      dev.deviceSpecificSettings = QVariant::fromValue(s);

      QByteArray bytes;
      {
        DataStreamReader r{&bytes};
        r.readFrom(dev);
      }
      bytes += int32_bytes(0x12345678);

      DataStreamWriter w{bytes};
      Device::DeviceSettings got;
      REQUIRE_NOTHROW(w.writeTo(got));
      const auto midi = got.deviceSpecificSettings.value<Protocols::MIDISpecificSettings>();
      checkSame(midi, s);
      CHECK(midi.controllers == mode);
      int32_t a{};
      w.stream().stream >> a;
      CHECK(a == 0x12345678);
    }
  });
}
