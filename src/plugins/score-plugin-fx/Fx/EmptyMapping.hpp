#pragma once
#include <Fx/Types.hpp>

#include <ossia/dataflow/port.hpp>

#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

namespace Nodes
{
// FIXME all incorrect when token_request smaller than tick
namespace EmptyValueMapping
{
struct Node
{
  halp_meta(name, "Empty value mapper")
  halp_meta(c_name, "EmptyValueMapper")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(description, "Copies its inputs to its outputs")
  halp_meta(uuid, "70B12B42-BB4B-4A13-861B-53C577601186");

  struct
  {
    ossia_port<"in", ossia::value_port> port{};
  } inputs;
  struct
  {
    ossia_port<"out", ossia::value_port> port{};
  } outputs;

  void operator()() { outputs.port.value->set_data(inputs.port.value->get_data()); }
};
}

namespace EmptyMidiMapping
{
struct Node
{
  halp_meta(name, "Empty midi mapper")
  halp_meta(c_name, "EmptyMidiMapper")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(description, "Copies its inputs to its outputs")
  halp_meta(uuid, "2CE4F3F3-E04F-48CD-B81C-1F6537EC8CFA");

  struct
  {
    ossia_port<"in", ossia::midi_port> port{};
  } inputs;
  struct
  {
    ossia_port<"out", ossia::midi_port> port{};
  } outputs;

  void operator()() { outputs.port.value->messages = inputs.port.value->messages; }
};
}

namespace EmptyAudioMapping
{
struct Node
{
  halp_meta(name, "Empty audio mapper")
  halp_meta(c_name, "EmptyAudioMapper")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(description, "Copies its inputs to its outputs")
  halp_meta(uuid, "D074CC6C-D1CB-47F8-871D-CC949D8EEBEC");

  struct
  {
    ossia_port<"in", ossia::audio_port> port{};
  } inputs;
  struct
  {
    ossia_port<"out", ossia::audio_port> port{};
  } outputs;

  void operator()() { *outputs.port.value = *inputs.port.value; }
};
}
}
