#pragma once
#include <halp/callback.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

namespace Nodes::PitchToValue
{
struct Node
{
  halp_meta(name, "Midi Pitch")
  halp_meta(c_name, "PitchToValue")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-utilities.html#midi-pitch")
  halp_meta(description, "Extract a MIDI pitch")
  halp_meta(uuid, "29ce484f-cb56-4501-af79-88768fa261c3")

  struct
  {
    halp::midi_bus<"in", libremidi::message> midi;
  } inputs;
  struct
  {
    struct : halp::timed_callback<"out", int>
    {
      halp_meta(unit, "midipitch");
    } out;
  } outputs;

  void operator()()
  {
    for(const auto& note : inputs.midi)
    {
      if(note.get_message_type() == libremidi::message_type::NOTE_ON)
        outputs.out(note.timestamp, (int)note.bytes[1]);
    }
  }
};
}
