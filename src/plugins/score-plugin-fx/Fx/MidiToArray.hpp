#pragma once
#include <Fx/Types.hpp>

#include <ossia/dataflow/port.hpp>

#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

namespace Nodes::MidiToArray
{
struct Node
{
  halp_meta(name, "MIDI to array")
  halp_meta(c_name, "MidiToArray")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(description, "Converts a MIDI 1 input to a sequence of bytes")
  halp_meta(uuid, "1311a0fb-3468-4e35-b0ba-c5aaf20efe7a");

  enum midi_mode
  {
    MIDI1,
    UMP
  };

  struct
  {
    ossia_port<"in", ossia::midi_port> port{};
    halp::enum_t<midi_mode, "Mode"> mode;
  } inputs;
  struct
  {
    ossia_port<"out", ossia::value_port> port{};
  } outputs;

  void operator()()
  {
    if(inputs.mode == midi_mode::MIDI1)
    {
      for(auto& msg : inputs.port.value->messages)
      {
        std::vector<ossia::value> vals;
        vals.reserve(4);
        for(auto byte : libremidi::midi1_from_ump(msg).bytes)
          vals.emplace_back((int)byte);
        outputs.port.value->write_value(std::move(vals), msg.timestamp);
      }
    }
    else
    {
      for(auto& msg : inputs.port.value->messages)
      {
        std::vector<ossia::value> vals;
        vals.reserve(4);
        for(std::size_t i = 0; i < msg.size(); i++)
          vals.emplace_back((int)msg.data[i]);
        outputs.port.value->write_value(std::move(vals), msg.timestamp);
      }
    }
  }
};
}
