#pragma once
#include <Process/Dataflow/MidiSysexArena.hpp>

#include <Vst/vst-compat.hpp>

#include <libremidi/cmidi2.hpp>
#include <libremidi/ump.hpp>

#include <cstdint>
#include <cstring>

namespace vst
{
/**
 * The VST 2 event `mess` makes at `deltaFrames`: a VstMidiEvent written into
 * `midi`, or, for the SysEx7 packet that ends a system-exclusive message, a
 * VstMidiSysexEvent written into `sysex` that points at the whole F0 ... F7
 * message in `arena`.
 *
 * Null when the packet makes none: a SysEx7 packet that does not end a
 * message, a SysEx8 one, a message with no MIDI 1 form, or the end of a
 * system-exclusive message when `sysex` is null (no slot left).
 */
inline VstEvent* toVstEvent(
    const libremidi::ump& mess, int32_t deltaFrames, VstMidiEvent& midi,
    VstMidiSysexEvent* sysex, Process::MidiSysexArena& arena,
    Process::MidiSysexArena::stream& stream) noexcept
{
  switch(mess.get_type())
  {
    case libremidi::midi2::message_type::SYSEX7: {
      const auto bytes = arena.feed(stream, mess);
      if(bytes.empty() || !sysex)
        return nullptr;

      std::memset(sysex, 0, sizeof(VstMidiSysexEvent));
      sysex->type = kVstSysExType;
      sysex->byteSize = sizeof(VstMidiSysexEvent);
      sysex->deltaFrames = deltaFrames;
      sysex->dumpBytes = int32_t(bytes.size());
      sysex->sysexDump = reinterpret_cast<char*>(const_cast<uint8_t*>(bytes.data()));
      return reinterpret_cast<VstEvent*>(sysex);
    }
    case libremidi::midi2::message_type::SYSEX8_MDS:
      return nullptr;
    default: {
      std::memset(&midi, 0, sizeof(VstMidiEvent));
      midi.type = kVstMidiType;
      midi.byteSize = sizeof(VstMidiEvent);
      midi.deltaFrames = deltaFrames;
      midi.flags = kVstMidiEventIsRealtime;
      if(cmidi2_convert_single_ump_to_midi1(
             (uint8_t*)midi.midiData, 4, (cmidi2_ump*)mess.data)
         <= 0)
        return nullptr;
      return reinterpret_cast<VstEvent*>(&midi);
    }
  }
}
}
