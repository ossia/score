#pragma once
#include <Process/Dataflow/MidiSysexArena.hpp>

#include <libremidi/ump.hpp>

#include <pluginterfaces/vst/ivstevents.h>

#include <optional>

namespace vst3
{
/**
 * The VST 3 data event for the system-exclusive message the SysEx7 packet
 * `mess` ends: type kMidiSysEx, pointing at the whole F0 ... F7 message, as
 * the VST 2 wrapper of the VST 3 SDK passes it on, in `arena`.
 *
 * Nothing when the packet ends no message, or is not a SysEx7 packet.
 */
inline std::optional<Steinberg::Vst::Event> toSysexEvent(
    const libremidi::ump& mess, Steinberg::int32 bus, Steinberg::int32 sampleOffset,
    Process::MidiSysexArena& arena, Process::MidiSysexArena::stream& stream) noexcept
{
  const auto bytes = arena.feed(stream, mess);
  if(bytes.empty())
    return std::nullopt;

  Steinberg::Vst::Event e{};
  e.busIndex = bus;
  e.sampleOffset = sampleOffset;
  e.ppqPosition = 0;
  e.flags = Steinberg::Vst::Event::kIsLive;
  e.type = Steinberg::Vst::Event::kDataEvent;
  e.data.type = Steinberg::Vst::DataEvent::kMidiSysEx;
  e.data.size = Steinberg::uint32(bytes.size());
  e.data.bytes = bytes.data();
  return e;
}
}
