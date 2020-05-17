#pragma once
#include <Engine/Node/PdNode.hpp>
namespace Nodes
{
namespace EmptyValueMapping
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Empty value mapper";
    static const constexpr auto objectKey = "EmptyValueMapper";
    static const constexpr auto category = "Mappings";
    static const constexpr auto author = "ossia score";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::Mapping;
    static const constexpr auto description = "Copies its inputs to its outputs";
    static const uuid_constexpr auto uuid = make_uuid("70B12B42-BB4B-4A13-861B-53C577601186");

    static const constexpr value_in value_ins[]{"in"};
    static const constexpr value_out value_outs[]{"out"};
  };

  using control_policy = ossia::safe_nodes::default_tick;
  static void
  run(const ossia::value_port& p1,
      ossia::value_port& p2,
      ossia::token_request,
      ossia::exec_state_facade)
  {
    p2.set_data(p1.get_data());
  }
};
}

namespace EmptyMidiMapping
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Empty midi mapper";
    static const constexpr auto objectKey = "EmptyMidiMapper";
    static const constexpr auto category = "Mappings";
    static const constexpr auto author = "ossia score";
    static const constexpr auto kind = Process::ProcessCategory::Mapping;
    static const constexpr auto description = "Copies its inputs to its outputs";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid = make_uuid("2CE4F3F3-E04F-48CD-B81C-1F6537EC8CFA");

    static const constexpr midi_in midi_ins[]{"in"};
    static const constexpr midi_out midi_outs[]{"out"};
  };

  using control_policy = ossia::safe_nodes::default_tick;
  static void
  run(const ossia::midi_port& p1,
      ossia::midi_port& p2,
      ossia::token_request,
      ossia::exec_state_facade)
  {
    p2.messages = p1.messages;
  }
};
}

namespace EmptyAudioMapping
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Empty audio mapper";
    static const constexpr auto objectKey = "EmptyAudioMapper";
    static const constexpr auto category = "Mappings";
    static const constexpr auto author = "ossia score";
    static const constexpr auto kind = Process::ProcessCategory::Mapping;
    static const constexpr auto description = "Copies its inputs to its outputs";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid = make_uuid("D074CC6C-D1CB-47F8-871D-CC949D8EEBEC");

    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr audio_out audio_outs[]{"out"};
  };

  using control_policy = ossia::safe_nodes::default_tick;
  static void
  run(const ossia::audio_port& p1,
      ossia::audio_port& p2,
      ossia::token_request,
      ossia::exec_state_facade)
  {
    p2.samples = p1.samples;
  }
};
}
}
