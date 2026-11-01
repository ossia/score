// MIDI display: its window is a time chooser, and the node sends it in
// seconds (a synced one is only in seconds on the execution side); malformed
// messages do not reach the note pairing; how messages are named.

#include <Ui/MidiDisplay.hpp>
#include <Ui/Monitor/MidiText.hpp>

#include <score_test/App.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("MIDI display: the node sends its window in seconds, and every message", "[ui][midi_display]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Ui::MidiDisplay::Node node;
    node.prepare({.input_channels = 0, .output_channels = 0, .frames = 64, .rate = 1000.});
    // A synced two bars at 120 BPM, as the binding hands it over
    node.inputs.window.value = 4.f;
    node.inputs.window.sync = true;
    halp::midi_msg m;
    m.bytes = {0x90, 60, 100}; // note on
    node.inputs.midi.midi_messages.push_back(m);

    halp::tick_musical tk{};
    tk.frames = 64;
    node.monitor->viewers = 1;
    node(tk);

    Ui::MidiDisplay::HeldNotes held;
    REQUIRE(node.monitor->held.consume(held));
    CHECK(held.window == 4.f);
    CHECK(held.held == 1);
    CHECK(held.test(0, 60));

    int events = 0;
    node.monitor->events.drain([&](const Ui::MidiDisplay::MidiEvent& e) {
      CHECK(e.status == m.bytes[0]);
      CHECK(e.d1 == 60);
      events++;
    });
    CHECK(events == 1);
  });
}

TEST_CASE("MIDI display: malformed messages are logged, not paired", "[ui][midi_display]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Ui::MidiDisplay::Node node;
    node.prepare({.input_channels = 0, .output_channels = 0, .frames = 64, .rate = 1000.});
    node.monitor->viewers = 1;
    // Data bytes with the high bit set, which index past the 128 notes of a
    // channel, and a data byte where the status should be
    for(auto bytes : {std::vector<uint8_t>{0x90, 200, 100}, {0x80, 0xFF, 0},
                      {0x9F, 60, 0x80}, {0x45, 60, 100}, {0xF8}})
    {
      halp::midi_msg m;
      m.bytes.assign(bytes.begin(), bytes.end());
      node.inputs.midi.midi_messages.push_back(m);
    }
    halp::tick_musical tk{};
    tk.frames = 64;
    node(tk);

    Ui::MidiDisplay::HeldNotes held;
    REQUIRE(node.monitor->held.consume(held));
    CHECK(held.held == 0);

    int events = 0;
    node.monitor->events.drain([&](const Ui::MidiDisplay::MidiEvent& e) {
      CHECK(e.flags == 0);
      events++;
    });
    CHECK(events == 5);
  });
}

TEST_CASE("MIDI display: what a status byte is", "[ui][midi_display]")
{
  using T = libremidi::message_type;
  using namespace Ui::Monitor;
  CHECK(midiMessageType(0x90, 60, 100) == T::NOTE_ON);
  // A note-on of velocity 0 is a note-off
  CHECK(midiMessageType(0x93, 60, 0) == T::NOTE_OFF);
  CHECK(midiMessageType(0xA0, 60, 10) == T::POLY_PRESSURE);
  CHECK(midiMessageType(0x90, 128, 100) == T::INVALID);
  CHECK(midiMessageType(0x7F, 0, 0) == T::INVALID);
  CHECK(midiMessageType(0xF8, 0, 0) == T::TIME_CLOCK);
  // The low nibble of a system message is not a channel
  CHECK(!isChannelMessage(midiMessageType(0xF8, 0, 0)));
  CHECK(isChannelMessage(midiMessageType(0xE5, 0, 64)));

  CHECK(midiNoteName(60) == QStringLiteral("C4"));
  CHECK(midiNoteName(200) == QStringLiteral("?"));
  CHECK(describeMidi(0x90, 60, 100, 3, false) == QStringLiteral("ch1 Note On 60 (C4) vel 100"));
  CHECK(describeMidi(0x90, 200, 100, 3, false).startsWith(QStringLiteral("Invalid")));
  CHECK(midiBytesText(0x9A, 0x3C, 0x7F, 3) == QStringLiteral("9A 3C 7F"));
}
