#pragma once
#include <Process/Dataflow/MidiSysexArena.hpp>

#include <libremidi/ump.hpp>

#include <clap/events.h>

#include <optional>

namespace Clap
{
/**
 * The CLAP_EVENT_MIDI_SYSEX event for the system-exclusive message the SysEx7
 * packet `mess` ends, pointing at the whole F0 ... F7 message in `arena`.
 *
 * Nothing when the packet ends no message, or is not a SysEx7 packet.
 */
inline std::optional<clap_event_midi_sysex_t> toSysexEvent(
    const libremidi::ump& mess, uint32_t time, uint16_t port_index,
    Process::MidiSysexArena& arena, Process::MidiSysexArena::stream& stream) noexcept
{
  const auto bytes = arena.feed(stream, mess);
  if(bytes.empty())
    return std::nullopt;

  clap_event_midi_sysex_t ev{};
  ev.header.size = sizeof(clap_event_midi_sysex_t);
  ev.header.time = time;
  ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  ev.header.type = CLAP_EVENT_MIDI_SYSEX;
  ev.header.flags = 0;
  ev.port_index = port_index;
  ev.buffer = bytes.data();
  ev.size = uint32_t(bytes.size());
  return ev;
}
}
