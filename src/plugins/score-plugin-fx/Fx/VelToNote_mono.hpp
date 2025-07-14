#pragma once
#include <Fx/Types.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
namespace Nodes
{
namespace PulseToNoteMono
{
struct Node
{
  halp_meta(name, "Pulse to Midi (mono)")
  halp_meta(c_name, "PulseToNoteMono")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/midi-utilities.html#pulse-to-note")
  halp_meta(
      description,
      "Converts a message into MIDI.\n"
      "If the input is an impulse, the output will be the default pitch "
      "at the default velocity.\n"
      "If the input is a single integer in [0; 127], the output will be "
      "the relevant note at the default velocity"
      "If the input is an array of two values between [0; 127], the "
      "output will be the relevant note.")

  halp_meta(uuid, "6f739723-7b14-4532-a91a-0423ed332ebe")

  struct
  {
    halp::val_port<"in", std::optional<int>> port{};
    halp::spinbox_i32<"duration", halp::range{1, 100000, 100}> duration{};

  } inputs;
  struct
  {
    halp::midi_out_bus<"out", halp::midi_msg> midi;
  } outputs;

  int cur_note = -1;
  int cur_note_start = 0;
  int cur_frames = 0;

  static constexpr uint8_t midi_clamp(int num)
  {
    return (uint8_t)ossia::clamp(num, 0, 127);
  }

  void operator()(int frames)
  {
    if(inputs.port.value)
    {
      if(cur_note != -1)
      {
        outputs.midi.note_off(1, cur_note, 0).timestamp = 0;
      }
      cur_note = *inputs.port.value;
      cur_note_start = cur_frames;
      outputs.midi.note_on(1, cur_note, 127).timestamp = 0;
    }

    if(cur_note != -1 && cur_frames - cur_note_start > inputs.duration)
    {
      outputs.midi.note_off(1, cur_note, 0).timestamp = 0;
      cur_note = -1;
    }

    cur_frames += frames;
  }
};
}
}
