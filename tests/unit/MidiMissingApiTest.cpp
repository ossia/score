// =============================================================================
// A5 — loading a document whose MIDI device names an API this build does not
// have must NOT abort the application.
//
// THE DEFECT. Protocols/MIDI/MIDIDevice.cpp's makeInputConfiguration did
//     case libremidi::API::ALSA_SEQ: {
//       auto ptr = get_if<libremidi::alsa_seq::input_configuration>(&api_conf);
//       SCORE_ASSERT(ptr);
// but libremidi::midi_in_configuration_for(api) only fills the variant for a
// backend that was COMPILED IN (libremidi.cpp -> midi_any::for_backend, which
// walks available_backends). On a build without ALSA — macOS, Windows, WASM, or
// any Linux build made without it — the variant holds unspecified_configuration,
// get_if returns null, and opening a Linux-authored document ABORTS. The same
// shape sat in the KEYBOARD case.
//
// That is an assert on EXTERNAL STATE: which MIDI backends exist is a property
// of the machine and of how score was built, not of the code being correct.
//
// THE BEHAVIOUR CHOSEN, and why. reconnect() now checks
// libremidi::available_apis() up front and REFUSES the device with a
// diagnostic, leaving it in score's ordinary "could not connect" state — the
// same state an unplugged interface or a busy OSC port produces. The rest of
// the document loads and plays.
//
// NOT a fallback to a null/dummy backend: a MIDI device is an I/O endpoint
// bound to a named external port. A silent stand-in would make the document
// look like it works while every note goes nowhere — trading a loud failure for
// a silent one — and it would rewrite the document's requested API on the next
// save, so the document would no longer say what the author meant. A missing
// backend must degrade visibly.
//
// This test only means something on a machine that genuinely lacks the API, so
// it asks libremidi rather than assuming, and SKIPs where ALSA_SEQ is present.
// (On the reference machine libremidi is built with JACK, PipeWire and KEYBOARD
// and WITHOUT ALSA, so it runs.)
//
//   ctest -R unit_midi_missing_api --output-on-failure
// =============================================================================
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Protocols/MIDI/MIDIDevice.hpp>
#include <Protocols/MIDI/MIDISpecificSettings.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <libremidi/api.hpp>


#include <QApplication>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

namespace
{
bool api_available(libremidi::API api)
{
  for(auto a : libremidi::available_apis())
    if(a == api)
      return true;
  for(auto a : libremidi::available_ump_apis())
    if(a == api)
      return true;
  return false;
}

// Protocols::MIDIInputProtocolFactory
UuidKey<Device::ProtocolFactory> midi_in_protocol()
{
  return UuidKey<Device::ProtocolFactory>::fromString(
      std::string{"f5e04ef0-16dd-4997-8f81-f5a04b8702bc"});
}
} // namespace

TEST_CASE(
    "a document whose MIDI device names an unavailable API loads, and leaves "
    "that device disconnected",
    "[unit][midi][protocols]")
{
  if(api_available(libremidi::API::ALSA_SEQ))
    SKIP("this build of libremidi HAS the ALSA sequencer backend, so the "
         "missing-backend path cannot be exercised here");

  // The direct pin. This is the function that carried the SCORE_ASSERT: given
  // settings naming a backend that is not compiled in, building the
  // configuration must come back, not abort. Driven directly because whether
  // MIDIDevice::reconnect() even reaches it depends on the machine having some
  // OTHER MIDI port for locateDevice's observer to enumerate — the branch is
  // reachable in the field but not reliably in a test rig.
  {
    CHECK_FALSE(Protocols::midiApiAvailable(libremidi::API::ALSA_SEQ));

    Protocols::MIDISpecificSettings midi;
    midi.io = Protocols::MIDISpecificSettings::IO::In;
    midi.handle.api = libremidi::API::ALSA_SEQ;

    // Pre-fix: SCORE_ASSERT(get_if<alsa_seq::input_configuration>(...)) on a
    // null pointer -> abort. Post-fix: the generic configuration comes back.
    // No device needed: `self` is only used to install the keyboard filter.
    auto [conf, api_conf] = Protocols::makeInputConfiguration(nullptr, midi);
    (void)conf;
    CHECK(get_if<libremidi::alsa_seq::input_configuration>(&api_conf) == nullptr);
  }

  struct Result
  {
    bool ran = false;
    bool protocol_present = false;
    bool device_present = false;
    bool connected = true;
  } out;

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto& protocols = ctx.interfaces<Device::ProtocolFactoryList>();
    if(!protocols.get(midi_in_protocol()))
      return; // score-plugin-protocols not built: reported below
    out.protocol_present = true;

    // Exactly what a Linux-authored document deserializes into: an ALSA
    // sequencer input port that this machine cannot possibly provide.
    Protocols::MIDISpecificSettings midi;
    midi.io = Protocols::MIDISpecificSettings::IO::In;
    midi.handle.api = libremidi::API::ALSA_SEQ;
    midi.handle.device_name = "Some ALSA client";
    midi.handle.port_name = "MIDI 1";
    midi.handle.display_name = "Some ALSA client:MIDI 1";

    Device::DeviceSettings set;
    set.name = QStringLiteral("AlsaSeqIn");
    set.protocol = midi_in_protocol();
    set.deviceSpecificSettings = QVariant::fromValue(midi);

    // The path a document load takes (Explorer::Command::LoadDevice ->
    // DeviceDocumentPlugin::loadDevice -> MIDIDevice::reconnect). Pre-fix this
    // call ABORTS the process, which is the whole point.
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Explorer::Command::LoadDevice{plug, set});
    QApplication::processEvents();
    QApplication::processEvents();

    // We are still alive: that is assertion number one.
    out.ran = true;

    for(auto& dev : plug.list().devices())
    {
      if(dev->settings().name == set.name)
      {
        out.device_present = true;
        out.connected = dev->connected();
      }
    }
  });

  if(!out.protocol_present)
    SKIP("score-plugin-protocols is not built here");

  // 1. Loading it did not abort.
  REQUIRE(out.ran);

  // 2. The device is still part of the document — refusing to connect must not
  //    delete the user's device, or saving would drop it.
  CHECK(out.device_present);

  // 3. ...and it is visibly not connected, rather than pretending to work.
  CHECK_FALSE(out.connected);
}
