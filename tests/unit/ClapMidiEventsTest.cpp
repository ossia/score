// The CLAP event a plug-in taking the MIDI dialect gets for a
// system-exclusive message a graph port carries as SysEx7 packets: one
// CLAP_EVENT_MIDI_SYSEX pointing at the whole F0 ... F7 message, in storage
// that lasts until the next process call.
#include "MidiSysexTestPackets.hpp"

#include <Clap/MidiEvents.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{
std::vector<uint8_t> bytes_of(const clap_event_midi_sysex_t& e)
{
  REQUIRE(e.header.type == CLAP_EVENT_MIDI_SYSEX);
  REQUIRE(e.header.space_id == CLAP_CORE_EVENT_SPACE_ID);
  REQUIRE(e.header.size == sizeof(clap_event_midi_sysex_t));
  return {e.buffer, e.buffer + e.size};
}
}

TEST_CASE("CLAP: a sysex message is one CLAP_EVENT_MIDI_SYSEX", "[clap][sysex]")
{
  Process::MidiSysexArena arena;
  Process::MidiSysexArena::stream stream;

  const auto mts = sysex_test::mts_message();
  std::vector<clap_event_midi_sysex_t> events;
  for(const auto& p : sysex_test::packets_of(mts))
    if(auto e = Clap::toSysexEvent(p, 9, 2, arena, stream))
      events.push_back(*e);

  REQUIRE(events.size() == 1);
  CHECK(bytes_of(events[0]) == mts);
  CHECK(events[0].header.time == 9);
  CHECK(events[0].port_index == 2);

  CHECK(!Clap::toSysexEvent(sysex_test::note_on(1, 60, 100), 0, 0, arena, stream));
}

TEST_CASE("CLAP: every message of an MTS tuning in one call stays intact", "[clap][sysex]")
{
  Process::MidiSysexArena arena;
  Process::MidiSysexArena::stream stream;

  std::vector<std::vector<uint8_t>> sent;
  std::vector<clap_event_midi_sysex_t> events;
  for(int key = 0; key < 128; key += 4)
  {
    sent.push_back(sysex_test::mts_message(uint8_t(key)));
    for(const auto& p : sysex_test::packets_of(sent.back()))
      if(auto e = Clap::toSysexEvent(p, 0, 0, arena, stream))
        events.push_back(*e);
  }

  REQUIRE(events.size() == 32);
  for(std::size_t i = 0; i < 32; i++)
    CHECK(bytes_of(events[i]) == sent[i]);
}
