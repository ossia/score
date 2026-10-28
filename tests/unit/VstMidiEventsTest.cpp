// The VST 2 events a VST 2 synth gets for what a graph port carries: a
// system-exclusive message, carried as SysEx7 packets, is one
// VstMidiSysexEvent pointing at the whole F0 ... F7 message, in storage that
// lasts until the next process call; channel messages are VstMidiEvents.
#include "MidiSysexTestPackets.hpp"

#include <Vst/MidiEvents.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

namespace
{
//! What Node.hpp's dispatchMidi hands the plug-in for `packets`, in order.
struct process_call
{
  Process::MidiSysexArena& arena;
  Process::MidiSysexArena::stream& stream;
  std::array<VstMidiSysexEvent, 128>& sysex_slots;
  std::vector<VstMidiEvent> midi = std::vector<VstMidiEvent>(512);
  std::vector<VstEvent*> events;

  void run(const std::vector<libremidi::ump>& packets, int32_t delta = 0)
  {
    arena.clear();
    events.clear();
    std::size_t sysex_count = 0;
    for(const auto& p : packets)
    {
      VstMidiSysexEvent* sysex
          = sysex_count < sysex_slots.size() ? &sysex_slots[sysex_count] : nullptr;
      VstEvent* e = vst::toVstEvent(p, delta, midi[events.size()], sysex, arena, stream);
      if(!e)
        continue;
      if(e == reinterpret_cast<VstEvent*>(sysex))
        sysex_count++;
      events.push_back(e);
    }
  }
};

std::vector<uint8_t> dump_of(const VstEvent* e)
{
  REQUIRE(e->type == kVstSysExType);
  auto* sx = reinterpret_cast<const VstMidiSysexEvent*>(e);
  CHECK(sx->byteSize == int32_t(sizeof(VstMidiSysexEvent)));
  return {sx->sysexDump, sx->sysexDump + sx->dumpBytes};
}
}

TEST_CASE("VST 2: a sysex message is one VstMidiSysexEvent", "[vst][sysex]")
{
  Process::MidiSysexArena arena;
  Process::MidiSysexArena::stream stream;
  std::array<VstMidiSysexEvent, 128> slots{};
  process_call call{arena, stream, slots};

  const auto mts = sysex_test::mts_message();
  auto packets = sysex_test::packets_of(mts);
  REQUIRE(packets.size() == 4);
  packets.push_back(sysex_test::note_on(1, 60, 100));

  call.run(packets, 17);
  REQUIRE(call.events.size() == 2);
  CHECK(dump_of(call.events[0]) == std::vector<uint8_t>(mts.begin(), mts.end()));
  CHECK(call.events[0]->deltaFrames == 17);

  // The channel message as before.
  REQUIRE(call.events[1]->type == kVstMidiType);
  auto* note = reinterpret_cast<const VstMidiEvent*>(call.events[1]);
  CHECK(uint8_t(note->midiData[0]) == 0x90);
  CHECK(uint8_t(note->midiData[1]) == 60);
  CHECK(uint8_t(note->midiData[2]) == 100);
  CHECK(note->deltaFrames == 17);
}

TEST_CASE("VST 2: the 32 messages of an MTS tuning in one call stay intact", "[vst][sysex]")
{
  Process::MidiSysexArena arena;
  Process::MidiSysexArena::stream stream;
  std::array<VstMidiSysexEvent, 128> slots{};
  process_call call{arena, stream, slots};

  std::vector<libremidi::ump> packets;
  std::vector<std::vector<uint8_t>> sent;
  for(int key = 0; key < 128; key += 4)
  {
    sent.push_back(sysex_test::mts_message(uint8_t(key)));
    for(auto& p : sysex_test::packets_of(sent.back()))
      packets.push_back(p);
  }

  call.run(packets);
  REQUIRE(call.events.size() == 32);
  for(std::size_t i = 0; i < 32; i++)
    CHECK(dump_of(call.events[i]) == sent[i]);
  CHECK(arena.dropped() == 0);
}

TEST_CASE("VST 2: a sysex message split across two ticks arrives whole", "[vst][sysex]")
{
  Process::MidiSysexArena arena;
  Process::MidiSysexArena::stream stream;
  std::array<VstMidiSysexEvent, 128> slots{};
  process_call call{arena, stream, slots};

  const auto mts = sysex_test::mts_message();
  const auto packets = sysex_test::packets_of(mts);
  call.run({packets[0], packets[1]});
  CHECK(call.events.empty());
  call.run({packets[2], packets[3]});
  REQUIRE(call.events.size() == 1);
  CHECK(dump_of(call.events[0]) == mts);
}

TEST_CASE("VST 2: past the storage of a call, a message is dropped, not cut", "[vst][sysex]")
{
  Process::MidiSysexArena arena;
  Process::MidiSysexArena::stream stream;
  std::array<VstMidiSysexEvent, 128> slots{};
  process_call call{arena, stream, slots};

  // 4000-byte messages: four fit in the 16384 bytes of one call.
  std::vector<uint8_t> big{0xF0};
  big.resize(3999, 0x11);
  big.push_back(0xF7);
  std::vector<libremidi::ump> packets;
  for(int i = 0; i < 5; i++)
    for(auto& p : sysex_test::packets_of(big))
      packets.push_back(p);

  call.run(packets);
  REQUIRE(call.events.size() == 4);
  for(auto* e : call.events)
    CHECK(dump_of(e) == big);
  CHECK(arena.dropped() == 1);

  // The next call has room again.
  call.run(sysex_test::packets_of(big));
  REQUIRE(call.events.size() == 1);
  CHECK(dump_of(call.events[0]) == big);
}
