#pragma once
// System-exclusive messages as a graph port carries them: SysEx7 packets.
#include <libremidi/detail/conversion.hpp>
#include <libremidi/ump.hpp>

#include <cstdint>
#include <vector>

namespace sysex_test
{
//! MIDI Tuning Standard, real-time single note tuning change of four keys
//! (60 to 63, each a quarter tone up): 24 bytes, four SysEx7 packets.
inline std::vector<uint8_t> mts_message(uint8_t first_key = 60)
{
  std::vector<uint8_t> m{0xF0, 0x7F, 0x7F, 0x08, 0x02, 0x00, 0x04};
  for(uint8_t k = first_key; k < first_key + 4; k++)
  {
    m.push_back(k);
    m.push_back(k);
    m.push_back(0x40);
    m.push_back(0x00);
  }
  m.push_back(0xF7);
  return m;
}

inline std::vector<libremidi::ump> packets_of(const std::vector<uint8_t>& sysex)
{
  libremidi::midi1_to_midi2 conv;
  std::vector<libremidi::ump> packets;
  conv.convert_packets(sysex.data(), sysex.size(), 0, [&](const libremidi::ump& p) {
    packets.push_back(p);
  });
  return packets;
}

inline libremidi::ump note_on(int channel, int note, int velocity)
{
  return libremidi::ump_from_midi1(libremidi::message{
      {uint8_t(0x90 | (channel - 1)), uint8_t(note), uint8_t(velocity)}, 0});
}
}
