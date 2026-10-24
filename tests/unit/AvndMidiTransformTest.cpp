#include <Advanced/MidiScaler/MidiTransform.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <map>
#include <vector>

using T = mtk::MidiTransform;

namespace
{
using bytes = std::vector<int>;

libremidi::message msg(std::initializer_list<unsigned char> b, int ts = 0)
{
  libremidi::message m{b};
  m.timestamp = ts;
  return m;
}

struct driver
{
  T fx;
  std::vector<bytes> out;
  std::vector<int64_t> ts;
  //! Sounding notes downstream, by status channel and pitch
  std::map<std::pair<int, int>, int> sounding;

  void in(const libremidi::message& m) { fx.inputs.midi.midi_messages.push_back(m); }

  std::vector<bytes> run(int frames = 64)
  {
    out.clear();
    ts.clear();
    fx.outputs.midi.midi_messages.clear();
    fx(halp::tick_musical{.frames = frames});
    for(auto& m : fx.outputs.midi)
    {
      CHECK(m.timestamp >= 0);
      CHECK((frames == 0 ? m.timestamp == 0 : m.timestamp < frames));
      out.push_back(bytes(m.bytes.begin(), m.bytes.end()));
      ts.push_back(m.timestamp);
      if(mtk::is_note_on(m))
        sounding[{m.bytes[0] & 0xF, m.bytes[1]}]++;
      else if(mtk::is_note_off(m))
      {
        auto& s = sounding[{m.bytes[0] & 0xF, m.bytes[1]}];
        CHECK(s > 0);
        s--;
      }
    }
    fx.inputs.midi.midi_messages.clear();
    return out;
  }

  int held() const
  {
    int n = 0;
    for(auto& [k, v] : sounding)
      n += v;
    return n;
  }
};
}

TEST_CASE("Midi transform defaults are an identity on notes", "[midi_transform]")
{
  driver d;
  d.in(msg({0x90, 60, 100}, 3));
  d.in(msg({0xB0, 1, 2}, 4));
  d.in(msg({0x90, 60, 0}, 5));
  CHECK(
      d.run()
      == std::vector<bytes>{{0x90, 60, 100}, {0xB0, 1, 2}, {0x80, 60, 0}});
  CHECK(d.ts == std::vector<int64_t>{3, 4, 5});
  CHECK(d.held() == 0);
}

TEST_CASE("Midi transform note to CC", "[midi_transform]")
{
  driver d;
  d.fx.inputs.into.value = T::CC;
  d.in(msg({0x91, 60, 100}));
  d.in(msg({0xF8}));
  d.in(msg({0x81, 60, 0}));
  CHECK(d.run() == std::vector<bytes>{{0xB1, 60, 100}, {0xF8}});

  SECTION("Insert after keeps the pair")
  {
    d.fx.inputs.mode.value = T::InsertAfter;
    d.in(msg({0x90, 62, 90}));
    d.in(msg({0x80, 62, 0}));
    CHECK(
        d.run()
        == std::vector<bytes>{{0x90, 62, 90}, {0xB0, 62, 90}, {0x80, 62, 0}});
  }

  SECTION("Insert before")
  {
    d.fx.inputs.mode.value = T::InsertBefore;
    d.in(msg({0x90, 62, 90}));
    CHECK(d.run() == std::vector<bytes>{{0xB0, 62, 90}, {0x90, 62, 90}});
  }

  SECTION("Drop removes the note and its note-off")
  {
    d.fx.inputs.mode.value = T::Drop;
    d.in(msg({0x90, 62, 90}));
    d.in(msg({0xB0, 1, 1}));
    d.in(msg({0x90, 62, 0}));
    CHECK(d.run() == std::vector<bytes>{{0xB0, 1, 1}});
  }

  SECTION("A mode change while held keeps the pair consistent")
  {
    d.fx.inputs.mode.value = T::InsertAfter;
    d.in(msg({0x90, 64, 90}));
    d.run();
    d.fx.inputs.mode.value = T::Replace;
    d.in(msg({0x80, 64, 0}));
    CHECK(d.run() == std::vector<bytes>{{0x80, 64, 0}});
    CHECK(d.held() == 0);
  }
}

TEST_CASE("Midi transform CC to note", "[midi_transform]")
{
  driver d;
  d.fx.inputs.what.value = T::CC;
  d.fx.inputs.into.value = T::NoteOn;
  d.fx.inputs.byte1_source.value = T::Arg1;
  d.fx.inputs.arg1.value = 60;

  SECTION("Value 0 ends the note")
  {
    d.in(msg({0xB0, 64, 127}));
    d.in(msg({0xB0, 64, 0}));
    CHECK(d.run() == std::vector<bytes>{{0x90, 60, 127}, {0x80, 60, 0}});
  }

  SECTION("Constant velocity: still ended by value 0, retriggered by a new value")
  {
    d.fx.inputs.byte2_source.value = T::Arg2;
    d.fx.inputs.arg2.value = 100;
    d.in(msg({0xB0, 64, 127}));
    d.in(msg({0xB0, 64, 90}));
    d.in(msg({0xB0, 64, 0}));
    CHECK(
        d.run()
        == std::vector<bytes>{
            {0x90, 60, 100}, {0x80, 60, 0}, {0x90, 60, 100}, {0x80, 60, 0}});
    CHECK(d.held() == 0);
  }

  SECTION("Two controllers on the same note do not cut each other")
  {
    d.in(msg({0xB0, 1, 100}));
    d.in(msg({0xB0, 2, 100}));
    d.in(msg({0xB0, 1, 0}));
    CHECK(d.run() == std::vector<bytes>{{0x90, 60, 100}});
    d.in(msg({0xB0, 2, 0}));
    CHECK(d.run() == std::vector<bytes>{{0x80, 60, 0}});
  }

  SECTION("CC 123 ends produced notes and passes untransformed")
  {
    d.in(msg({0xB0, 64, 127}));
    d.run();
    d.in(msg({0xB0, 123, 0}, 9));
    CHECK(d.run() == std::vector<bytes>{{0x80, 60, 0}, {0xB0, 123, 0}});
    CHECK(d.ts == std::vector<int64_t>{9, 9});
  }

  SECTION("Stop ends produced notes on the next tick")
  {
    d.in(msg({0xB0, 64, 127}));
    d.in(msg({0x90, 40, 100}));
    d.run();
    d.fx.stop();
    CHECK(d.run(0) == std::vector<bytes>{{0x80, 60, 0}, {0x80, 40, 0}});
    CHECK(d.run().empty());
    CHECK(d.held() == 0);
  }
}

TEST_CASE("Midi transform keyless sources retrigger", "[midi_transform]")
{
  driver d;
  d.fx.inputs.what.value = T::ProgramChange;
  d.fx.inputs.into.value = T::NoteOn;
  d.fx.inputs.byte2_source.value = T::Arg2;
  d.fx.inputs.arg2.value = 80;
  d.in(msg({0xC0, 10}));
  d.in(msg({0xC0, 12}));
  CHECK(d.run() == std::vector<bytes>{{0x90, 10, 80}, {0x80, 10, 0}, {0x90, 12, 80}});

  driver c;
  c.fx.inputs.what.value = T::Clock;
  c.fx.inputs.into.value = T::NoteOn;
  c.fx.inputs.byte1_source.value = T::Arg1;
  c.fx.inputs.byte2_source.value = T::Arg2;
  c.fx.inputs.arg1.value = 36;
  c.fx.inputs.arg2.value = 100;
  c.fx.inputs.channel_source.value = T::ChannelArg;
  c.fx.inputs.channel_arg.value = 10;
  c.in(msg({0xF8}));
  c.in(msg({0xF8}));
  CHECK(c.run() == std::vector<bytes>{{0x99, 36, 100}, {0x89, 36, 0}, {0x99, 36, 100}});
}

TEST_CASE("Midi transform note-off rules", "[midi_transform]")
{
  driver d;
  d.fx.inputs.what.value = T::NoteOff;
  d.fx.inputs.into.value = T::CC;
  d.fx.inputs.byte1_source.value = T::Arg1;
  d.fx.inputs.arg1.value = 20;
  d.in(msg({0x90, 60, 100}));
  d.in(msg({0x80, 60, 30}));
  // The note-on went out, so its note-off is kept and the CC added
  CHECK(
      d.run() == std::vector<bytes>{{0x90, 60, 100}, {0x80, 60, 30}, {0xB0, 20, 30}});
  // A stray note-off is replaced
  d.in(msg({0x80, 61, 30}));
  CHECK(d.run() == std::vector<bytes>{{0xB0, 20, 30}});

  d.fx.inputs.mode.value = T::Drop;
  d.in(msg({0x90, 60, 100}));
  d.in(msg({0x80, 60, 30}));
  d.in(msg({0x80, 61, 30}));
  CHECK(d.run() == std::vector<bytes>{{0x90, 60, 100}, {0x80, 60, 30}});
}

TEST_CASE("Midi transform note to note with a fixed pitch", "[midi_transform]")
{
  driver d;
  d.fx.inputs.byte1_source.value = T::Arg1;
  d.fx.inputs.arg1.value = 36;
  d.in(msg({0x90, 60, 100}));
  d.in(msg({0x90, 64, 100}));
  CHECK(d.run() == std::vector<bytes>{{0x90, 36, 100}});
  d.fx.inputs.arg1.value = 40;
  d.in(msg({0x80, 60, 0}));
  CHECK(d.run().empty());
  d.in(msg({0x80, 64, 0}));
  CHECK(d.run() == std::vector<bytes>{{0x80, 36, 0}});
  CHECK(d.held() == 0);
}

TEST_CASE("Midi transform input filter", "[midi_transform]")
{
  driver d;
  d.fx.inputs.into.value = T::CC;
  d.fx.inputs.channel.value = 2;
  d.fx.inputs.byte1_range.value = {60, 71};
  d.fx.inputs.byte2_range.value = {100, 127};
  d.fx.inputs.byte2_test.value = T::Outside;
  d.in(msg({0x91, 60, 50}));  // transformed
  d.in(msg({0x90, 60, 50}));  // channel 1
  d.in(msg({0x91, 72, 50}));  // byte 1 outside
  d.in(msg({0x91, 61, 110})); // byte 2 inside
  CHECK(
      d.run()
      == std::vector<bytes>{
          {0xB1, 60, 50}, {0x90, 60, 50}, {0x91, 72, 50}, {0x91, 61, 110}});

  d.fx.inputs.byte1_test.value = T::Outside;
  d.in(msg({0x91, 72, 50}));
  CHECK(d.run() == std::vector<bytes>{{0xB1, 72, 50}});
}

TEST_CASE("Midi transform sources and scaling", "[midi_transform]")
{
  driver d;
  d.fx.inputs.what.value = T::CC;
  d.fx.inputs.into.value = T::CC;
  d.fx.inputs.channel_source.value = T::DataByte2;
  d.fx.inputs.byte1_source.value = T::IncomingChannel;
  d.fx.inputs.byte2_source.value = T::ChannelArg;
  d.fx.inputs.channel_arg.value = 16;
  d.in(msg({0xBF, 1, 127}));
  d.in(msg({0xB0, 1, 0}));
  d.in(msg({0xB7, 1, 64}));
  CHECK(
      d.run()
      == std::vector<bytes>{{0xBF, 127, 127}, {0xB0, 0, 127}, {0xB8, 59, 127}});

  driver p;
  p.fx.inputs.what.value = T::CC;
  p.fx.inputs.into.value = T::PitchBend;
  p.fx.inputs.byte1_source.value = T::Arg1;
  p.in(msg({0xB2, 1, 64}));
  CHECK(p.run() == std::vector<bytes>{{0xE2, 0, 64}});
}

TEST_CASE("Midi transform real-time first", "[midi_transform]")
{
  driver d;
  d.fx.inputs.into.value = T::Clock;
  d.fx.inputs.mode.value = T::InsertAfter;
  d.in(msg({0x90, 60, 100}));
  CHECK(d.run() == std::vector<bytes>{{0x90, 60, 100}, {0xF8}});
  d.fx.inputs.realtime_first.value = true;
  d.in(msg({0x90, 61, 100}));
  CHECK(d.run() == std::vector<bytes>{{0xF8}, {0x90, 61, 100}});

  driver s;
  s.fx.inputs.what.value = T::Start;
  s.fx.inputs.into.value = T::CC;
  s.fx.inputs.mode.value = T::InsertBefore;
  s.fx.inputs.realtime_first.value = true;
  s.fx.inputs.byte1_source.value = T::Arg1;
  s.fx.inputs.byte2_source.value = T::Arg2;
  s.fx.inputs.arg1.value = 1;
  s.fx.inputs.arg2.value = 2;
  s.in(msg({0xFA}));
  CHECK(s.run() == std::vector<bytes>{{0xFA}, {0xB0, 1, 2}});
}

TEST_CASE("Midi transform ends a produced note when Into changes", "[midi_transform]")
{
  SECTION("Note to note on a fixed channel")
  {
    driver d;
    d.fx.inputs.channel_source.value = T::ChannelArg;
    d.fx.inputs.channel_arg.value = 2;
    d.in(msg({0x90, 60, 100}));
    d.run();
    CHECK(d.held() == 1);
    d.fx.inputs.into.value = T::CC;
    d.run();
    d.in(msg({0x80, 60, 0}));
    d.run();
    CHECK(d.held() == 0);
  }
  SECTION("CC to note")
  {
    driver d;
    d.fx.inputs.what.value = T::CC;
    d.fx.inputs.into.value = T::NoteOn;
    d.in(msg({0xB0, 64, 127}));
    d.run();
    CHECK(d.held() == 1);
    d.fx.inputs.into.value = T::CC;
    d.in(msg({0xB0, 64, 0}));
    d.run();
    CHECK(d.held() == 0);
  }
}

TEST_CASE("Midi transform note-offs follow their note-on when What changes", "[midi_transform]")
{
  driver d;
  d.fx.inputs.into.value = T::CC;
  d.in(msg({0x90, 60, 100}));
  CHECK(d.run() == std::vector<bytes>{{0xB0, 60, 100}});
  d.fx.inputs.what.value = T::CC;
  // The note-on was replaced: its note-off goes nowhere.
  d.in(msg({0x80, 60, 0}));
  CHECK(d.run().empty());
  d.fx.inputs.what.value = T::NoteOn;
  d.fx.inputs.mode.value = T::InsertAfter;
  d.in(msg({0x90, 60, 100}));
  d.run();
  d.in(msg({0x80, 60, 0}));
  d.run();
  CHECK(d.held() == 0);
}
