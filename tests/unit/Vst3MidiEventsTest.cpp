// The VST 3 event a plug-in gets for a system-exclusive message a graph port
// carries as SysEx7 packets: one DataEvent of type kMidiSysEx pointing at the
// whole F0 ... F7 message, in storage that lasts until the next process call.
#include "MidiSysexTestPackets.hpp"

#include <Vst3/MidiEvents.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{
std::vector<uint8_t> bytes_of(const Steinberg::Vst::Event& e)
{
  REQUIRE(e.type == Steinberg::Vst::Event::kDataEvent);
  REQUIRE(e.data.type == Steinberg::Vst::DataEvent::kMidiSysEx);
  return {e.data.bytes, e.data.bytes + e.data.size};
}
}

TEST_CASE("VST 3: a sysex message is one kMidiSysEx data event", "[vst3][sysex]")
{
  Process::MidiSysexArena arena;
  Process::MidiSysexArena::stream stream;

  const auto mts = sysex_test::mts_message();
  const auto packets = sysex_test::packets_of(mts);
  REQUIRE(packets.size() == 4);

  std::vector<Steinberg::Vst::Event> events;
  for(const auto& p : packets)
    if(auto e = vst3::toSysexEvent(p, 1, 42, arena, stream))
      events.push_back(*e);

  REQUIRE(events.size() == 1);
  CHECK(bytes_of(events[0]) == mts);
  CHECK(events[0].busIndex == 1);
  CHECK(events[0].sampleOffset == 42);
}

TEST_CASE("VST 3: channel messages are not data events", "[vst3][sysex]")
{
  Process::MidiSysexArena arena;
  Process::MidiSysexArena::stream stream;
  CHECK(!vst3::toSysexEvent(sysex_test::note_on(1, 60, 100), 0, 0, arena, stream));
}

TEST_CASE("VST 3: every message of an MTS tuning in one call stays intact", "[vst3][sysex]")
{
  Process::MidiSysexArena arena;
  Process::MidiSysexArena::stream stream;

  std::vector<std::vector<uint8_t>> sent;
  std::vector<Steinberg::Vst::Event> events;
  for(int key = 0; key < 128; key += 4)
  {
    sent.push_back(sysex_test::mts_message(uint8_t(key)));
    for(const auto& p : sysex_test::packets_of(sent.back()))
      if(auto e = vst3::toSysexEvent(p, 0, 0, arena, stream))
        events.push_back(*e);
  }

  REQUIRE(events.size() == 32);
  for(std::size_t i = 0; i < 32; i++)
    CHECK(bytes_of(events[i]) == sent[i]);
}
