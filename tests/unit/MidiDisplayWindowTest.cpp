// MIDI display: its window is a time chooser, and the node sends it in
// seconds (a synced one is only in seconds on the execution side).

#include <Ui/MidiDisplay.hpp>

#include <score_test/App.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("MIDI display: the node sends its window in seconds", "[ui][midi_display]")
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
    node(tk);
    REQUIRE(node.outputs.events.value);
    const auto& v = *node.outputs.events.value;
    REQUIRE(v.size() == 4 + 6); // header, one event
    CHECK(v[3] == 4.f);
    CHECK(v[4 + 2] == float(m.bytes[0])); // the event's status, after the header
  });
}
