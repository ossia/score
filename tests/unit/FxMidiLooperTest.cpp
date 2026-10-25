#include <Fx/Looper/MidiLooper.hpp>

#include <ossia/dataflow/execution_state.hpp>

#include <libremidi/reader.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include "AllocCounter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <map>
#include <set>
#include <tuple>
#include <vector>

using Catch::Approx;

namespace
{
using Looper = Nodes::MidiLooper::Node;
using LoopMode = Looper::LoopMode;
using Passthrough = Looper::Passthrough;
using OverdubMode = Looper::OverdubMode;
using HeldNotes = Looper::HeldNotes;

struct out_note
{
  int64_t at; //!< frame on the harness' count
  bool on;
  int channel;
  int pitch;
};

//! Drives the MIDI looper like the ossia binding does: one tick per buffer,
//! input timestamps relative to the tick.
struct midi_harness
{
  Looper node;
  ossia::execution_state st;
  int buffer_size{64};
  int64_t now{0};
  double tempo{120.};
  //! Ticks carry their place in the bars, 4/4 from the start, for the grid.
  bool musical{false};
  std::vector<libremidi::message> pending;
  std::vector<libremidi::message> controls_out;
  std::vector<out_note> out;

  explicit midi_harness(int bs = 64, int rate = 48000)
      : buffer_size{bs}
  {
    st.sampleRate = rate;
    st.bufferSize = bs;
    st.modelToSamplesRatio = 1.;
    st.samplesToModelRatio = 1.;
    node.ossia_state = {&st};
    node.inputs.quantif.value = 0.f;
    node.inputs.late.value = 0;
    node.inputs.grace.value = 0;
    node.inputs.passthrough = Passthrough::None;
    node.inputs.midi.midi_messages.reserve(64);
    node.outputs.midi.midi_messages.reserve(1024);
  }

  void note_on(int pitch, int at = 0, int vel = 100, int ch = 1)
  {
    auto m = libremidi::channel_events::note_on(ch, pitch, vel);
    m.timestamp = at;
    pending.push_back(m);
  }
  void note_off(int pitch, int at = 0, int ch = 1)
  {
    auto m = libremidi::channel_events::note_off(ch, pitch, 0);
    m.timestamp = at;
    pending.push_back(m);
  }
  void cc(int control, int value, int at = 0, int ch = 1)
  {
    auto m = libremidi::channel_events::control_change(ch, control, value);
    m.timestamp = at;
    pending.push_back(m);
  }

  ossia::token_request tick(int64_t frames)
  {
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{now};
    tk.date = ossia::time_value{now + frames};
    tk.start_sample = 0;
    tk.length_sample = frames;
    tk.tempo = tempo;
    if(musical)
    {
      const double qps = tempo / 60. / st.sampleRate;
      tk.musical_start_position = now * qps;
      tk.musical_end_position = (now + frames) * qps;
      tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
      tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
      // The engine's clock is at the end of the buffer while it runs.
      st.samples_since_start = now + frames;
    }
    return tk;
  }

  void run(int64_t frames = -1)
  {
    if(frames < 0)
      frames = buffer_size;
    auto& in = node.inputs.midi.midi_messages;
    in.clear();
    for(auto& m : pending)
      in.push_back(m);
    pending.clear();
    node.outputs.midi.midi_messages.clear();
    node(tick(frames));
    for(auto& m : node.outputs.midi.midi_messages)
    {
      const auto kind = mtk::classify(m);
      if(kind == mtk::midi_kind::note_on || kind == mtk::midi_kind::note_off)
        out.push_back(
            {now + m.timestamp, kind == mtk::midi_kind::note_on, mtk::channel_of(m),
             m.bytes[1]});
      else
        controls_out.push_back(m);
    }
    now += frames;
  }

  void run_frames(int64_t total)
  {
    while(total > 0)
    {
      const int64_t n = std::min<int64_t>(buffer_size, total);
      run(n);
      total -= n;
    }
  }

  std::vector<out_note> ons() const
  {
    std::vector<out_note> r;
    for(auto& n : out)
      if(n.on)
        r.push_back(n);
    return r;
  }
  std::vector<out_note> offs() const
  {
    std::vector<out_note> r;
    for(auto& n : out)
      if(!n.on)
        r.push_back(n);
    return r;
  }

  void press(auto& button)
  {
    button.value.emplace();
    run();
    button.value.reset();
  }
};

//! Records a loop of `len` frames with the notes `f` plays, then plays it.
void record(midi_harness& h, int64_t len, auto f)
{
  h.node.inputs.mode = LoopMode::Record;
  f(h);
  h.run_frames(len);
  h.node.inputs.mode = LoopMode::Play;
}
}

TEST_CASE("MIDI looper: a recorded phrase comes back at the same place every loop",
          "[fx][midi][looper]")
{
  midi_harness h;
  record(h, 640, [](midi_harness& h) {
    h.note_on(60, 10);
    h.note_off(60, 40);
  });
  // The first note-on was 10 frames into the first buffer, the note-off 40.
  // 3 more notes along the loop.
  REQUIRE(h.node.loop.note_count == 1);
  REQUIRE(h.node.loop.length == 640);

  const int64_t start = h.now;
  h.out.clear();
  h.run_frames(640 * 100);

  const auto ons = h.ons();
  const auto offs = h.offs();
  REQUIRE(ons.size() == 100);
  REQUIRE(offs.size() == 100);
  for(int i = 0; i < 100; i++)
  {
    CHECK(ons[i].at == start + i * 640 + 10);
    CHECK(offs[i].at == start + i * 640 + 40);
    CHECK(ons[i].pitch == 60);
  }
}

TEST_CASE("MIDI looper: a note held across the loop's end wraps or is truncated",
          "[fx][midi][looper]")
{
  for(auto held : {HeldNotes::Wrap, HeldNotes::Truncate})
  {
    midi_harness h;
    h.node.inputs.held = held;
    h.node.inputs.mode = LoopMode::Record;
    h.run_frames(500);
    h.note_on(64, 0); // at 500
    h.run_frames(140);
    // Recording ends at 640 with the key still down; let go 60 frames later.
    h.node.inputs.mode = LoopMode::Play;
    h.run(60);
    h.note_off(64, 0);
    h.run(4);
    REQUIRE(h.node.loop.length == 640);
    REQUIRE(h.node.loop.note_count == 1);
    CHECK(h.node.loop.notes[0].length == 200);

    h.out.clear();
    const int64_t start = h.now; // loop position 64
    h.run_frames(640 * 2);
    const auto ons = h.ons();
    const auto offs = h.offs();
    REQUIRE(ons.size() >= 1);
    REQUIRE(offs.size() >= 1);
    // The note-on is at loop position 500.
    CHECK(ons[0].at == start + (500 - 64));
    if(held == HeldNotes::Wrap)
      CHECK(offs.front().at == start + (60 - 64 + 640)); // position 60 of the next round
    else
      CHECK(offs.front().at == start + (640 - 64)); // the loop's end
  }
}

TEST_CASE("MIDI looper: a note played just before the recording belongs to its start",
          "[fx][midi][looper]")
{
  midi_harness h{64, 1000}; // a frame is a millisecond
  h.node.inputs.grace.value = 60;
  h.node.inputs.mode = LoopMode::Stop;
  h.note_on(60, 30); // 34 ms before the recording starts
  h.run();
  h.node.inputs.mode = LoopMode::Record;
  h.note_off(60, 20);
  h.run_frames(640);
  h.node.inputs.mode = LoopMode::Play;
  h.run();

  REQUIRE(h.node.loop.note_count == 1);
  CHECK(h.node.loop.notes[0].start == 0);
}

TEST_CASE("MIDI looper: replace silences a pitch of earlier passes where the pass went, "
          "merge keeps it",
          "[fx][midi][looper]")
{
  for(auto mode : {OverdubMode::Merge, OverdubMode::Replace})
  {
    midi_harness h;
    h.node.inputs.overdub_mode = mode;
    h.node.inputs.restart = Looper::Restart::Recording;
    record(h, 640, [](midi_harness& h) {
      h.note_on(60, 10);
      h.note_off(60, 20);
    });
    // Pass 2: over the whole loop, plays 60 at 300.
    h.node.inputs.mode = LoopMode::Overdub;
    h.run_frames(300);
    h.note_on(60, 0);
    h.run(10);
    h.note_off(60, 0);
    h.run_frames(330);
    h.node.inputs.mode = LoopMode::Play;
    h.run();

    h.out.clear();
    h.run_frames(640);
    const auto ons = h.ons();
    if(mode == OverdubMode::Merge)
      CHECK(ons.size() == 2);
    else
      CHECK(ons.size() == 1);
  }
}

TEST_CASE("MIDI looper: an overdub moving one controller leaves the others alone",
          "[fx][midi][looper]")
{
  midi_harness h;
  h.node.inputs.restart = Looper::Restart::Recording;
  record(h, 640, [](midi_harness& h) {
    h.cc(1, 10, 5);
    h.cc(74, 20, 6);
  });
  h.node.inputs.mode = LoopMode::Overdub;
  h.cc(74, 99, 5);
  h.run_frames(640);
  h.node.inputs.mode = LoopMode::Play;
  h.run();

  h.controls_out.clear();
  h.run_frames(640);
  int cc1 = 0, cc74_old = 0, cc74_new = 0;
  for(auto& m : h.controls_out)
  {
    if(m.bytes[1] == 1)
      cc1++;
    if(m.bytes[1] == 74 && m.bytes[2] == 20)
      cc74_old++;
    if(m.bytes[1] == 74 && m.bytes[2] == 99)
      cc74_new++;
  }
  CHECK(cc1 == 1);
  CHECK(cc74_old == 0);
  CHECK(cc74_new == 1);
}

TEST_CASE("MIDI looper: undo hides a pass and redo brings it back", "[fx][midi][looper]")
{
  midi_harness h;
  h.node.inputs.restart = Looper::Restart::Recording;
  record(h, 640, [](midi_harness& h) {
    h.note_on(60, 10);
    h.note_off(60, 20);
  });
  h.node.inputs.mode = LoopMode::Overdub;
  h.note_on(67, 10);
  h.note_off(67, 20);
  h.run_frames(640);
  h.node.inputs.mode = LoopMode::Play;
  h.run();
  REQUIRE(h.node.loop.count == 2);

  h.press(h.node.inputs.undo);
  h.out.clear();
  h.run_frames(640);
  CHECK(h.ons().size() == 1);

  h.press(h.node.inputs.redo);
  h.out.clear();
  h.run_frames(640);
  CHECK(h.ons().size() == 2);
}

TEST_CASE("MIDI looper: quantize pulls the notes to the grid and keeps the recording",
          "[fx][midi][looper]")
{
  midi_harness h{64, 48000};
  // A sixteenth at 120 bpm and 48 kHz is 6000 frames.
  h.node.inputs.grid.value = 1.f / 16.f;
  h.node.inputs.strength.value = 1.f;
  h.node.inputs.mode = LoopMode::Record;
  h.run_frames(5900);
  h.note_on(60, 0); // at 5900, 100 frames early
  h.run(64);
  h.note_off(60, 0);
  h.run_frames(24000 - 5964);
  h.node.inputs.mode = LoopMode::Play;
  h.run();
  REQUIRE(h.node.loop.length == 24000);

  h.out.clear();
  const int64_t start = h.now; // loop position 64
  h.run_frames(24000);
  const auto ons = h.ons();
  REQUIRE(ons.size() == 1);
  CHECK(ons[0].at == start + 6000 - 64);
  CHECK(h.node.loop.notes[0].start == 5900);
}

TEST_CASE("MIDI looper: stopping the process ends the notes the loop was playing",
          "[fx][midi][looper]")
{
  midi_harness h;
  h.node.inputs.mode = LoopMode::Record;
  h.note_on(60, 10);
  h.run_frames(576);
  h.note_off(60, 24); // frame 600
  h.run_frames(64);
  h.node.inputs.mode = LoopMode::Play;
  h.out.clear();
  h.run(64); // plays the note-on
  REQUIRE(h.ons().size() == 1);

  h.node.stop();
  // The host runs one empty tick after the process stops.
  ossia::token_request tk = h.tick(0);
  h.node.outputs.midi.midi_messages.clear();
  h.node(tk);
  int offs = 0;
  for(auto& m : h.node.outputs.midi.midi_messages)
    offs += mtk::classify(m) == mtk::midi_kind::note_off;
  CHECK(offs == 1);
}

TEST_CASE("MIDI looper: nothing is allocated while recording, overdubbing and playing",
          "[fx][midi][looper]")
{
  midi_harness h;
  h.node.inputs.passthrough = Passthrough::Full;
  h.pending.reserve(64);
  h.out.reserve(100000);
  h.controls_out.reserve(100000);

  int allocated = 0;
  {
    alloc_counter::scope counter;
    h.node.inputs.mode = LoopMode::Record;
    for(int i = 0; i < 20; i++)
    {
      h.note_on(60 + i % 12, 3);
      h.cc(1, i, 4);
      h.note_off(60 + i % 12, 50);
      h.run();
    }
    h.node.inputs.mode = LoopMode::Overdub;
    for(int i = 0; i < 40; i++)
    {
      h.note_on(48 + i % 12, 3);
      h.note_off(48 + i % 12, 50);
      h.run();
    }
    h.node.inputs.mode = LoopMode::Play;
    for(int i = 0; i < 40; i++)
      h.run();
    h.press(h.node.inputs.undo);
    h.press(h.node.inputs.redo);
    h.press(h.node.inputs.clear);
    allocated = counter.allocations();
  }
  CHECK(allocated == 0);
}

TEST_CASE("MIDI looper: a take is a MIDI file as long as the loop, at 120 bpm",
          "[fx][midi][looper][takes]")
{
  Nodes::Looper::midi_export e;
  e.rate = 48000.;
  e.tempo = 90.;
  // Four bars at 90 bpm: 16 quarters.
  const double fpq = 48000. * 60. / 90.;
  e.length = int64_t(16 * fpq);
  e.notes[0] = {.start = 0, .length = int64_t(fpq), .channel = 1, .pitch = 60, .velocity = 100, .pass = 0, .replaced_by = -1};
  e.notes[1] = {.start = int64_t(15.5 * fpq), .length = int64_t(fpq), .channel = 2, .pitch = 62, .velocity = 90, .pass = 0, .replaced_by = -1};
  e.note_count = 2;

  const auto bytes = e.to_smf();
  libremidi::reader r{true};
  std::vector<uint8_t> raw(bytes.begin(), bytes.end());
  REQUIRE(r.parse(raw) != libremidi::reader::invalid);
  REQUIRE(r.tracks.size() == 1);
  CHECK(r.startingTempo == Approx(120.));
  CHECK(r.ticksPerBeat == Approx(480.));
  // 16 quarters at 120 bpm: 8 seconds of score's time.
  CHECK(r.get_end_time() == Approx(16. * 480.));

  int ons = 0;
  int last_on_tick = -1;
  for(auto& ev : r.tracks[0])
  {
    if(mtk::classify(ev.m) == mtk::midi_kind::note_on)
    {
      ons++;
      last_on_tick = ev.tick;
    }
  }
  // The second note wraps: its tail is a note at the start.
  CHECK(ons == 3);
  CHECK(last_on_tick == int(15.5 * 480));
}

TEST_CASE("MIDI looper: capture makes a loop of the last bars played",
          "[fx][midi][looper][capture]")
{
  midi_harness h{64, 1000}; // a bar is 2000 frames
  h.node.inputs.capture_bars.value = 1;
  h.node.inputs.mode = LoopMode::Stop;
  h.run_frames(448);
  h.note_on(60, 52); // frame 500
  h.run();
  h.run_frames(128);
  h.note_off(60, 60); // frame 700
  h.run();
  h.run_frames(768);
  h.note_on(64, 12); // frame 1484
  h.run();
  h.note_off(64, 20);
  h.run();
  while(h.now < 2048)
    h.run();

  h.node.inputs.capture.value.emplace();
  h.run();
  h.node.inputs.capture.value.reset();

  REQUIRE(h.node.loop.length == 2000);
  REQUIRE(h.node.loop.note_count == 2);
  // The loop is frames 48 to 2048.
  CHECK(h.node.loop.notes[0].start == 500 - 48);
  CHECK(h.node.loop.notes[0].length == 200);
  CHECK(h.node.loop.notes[1].start == 1484 - 48);
  CHECK(h.node.transport.actualMode == LoopMode::Play);
}

namespace
{
//! Stops the process the way the host does: the notes still sounding end in
//! one more, empty tick.
void stop_and_collect(midi_harness& h)
{
  h.node.stop();
  h.node.outputs.midi.midi_messages.clear();
  h.node(h.tick(0));
  for(auto& m : h.node.outputs.midi.midi_messages)
    if(mtk::classify(m) == mtk::midi_kind::note_off)
      h.out.push_back({h.now, false, 1, m.bytes[1]});
}

//! Every note-on has its note-off, by pitch.
bool balanced(const std::vector<out_note>& out)
{
  std::map<int, int> s;
  for(auto& n : out)
    s[n.pitch] += n.on ? 1 : -1;
  for(auto& [p, c] : s)
    if(c != 0)
      return false;
  return true;
}
}

TEST_CASE("MIDI looper: a note held longer than the loop ends once it is let go",
          "[fx][midi][looper]")
{
  midi_harness h;
  h.node.inputs.passthrough = Passthrough::Full;
  h.node.inputs.mode = LoopMode::Record;
  h.note_on(60, 10);
  h.run_frames(640);
  h.node.inputs.mode = LoopMode::Play;
  h.run_frames(640 * 3); // still held
  h.note_off(60, 5);
  h.run();
  h.run_frames(640 * 3);
  stop_and_collect(h);
  CHECK(balanced(h.out));
}

TEST_CASE("MIDI looper: a note replaced while it sounds still ends",
          "[fx][midi][looper]")
{
  midi_harness h;
  h.node.inputs.overdub_mode = OverdubMode::Replace;
  h.node.inputs.restart = Looper::Restart::Recording;
  record(h, 640, [](midi_harness& h) {
    h.note_on(60, 10);
    h.note_off(60, 50);
  });
  h.run();
  h.out.clear();
  h.node.inputs.mode = LoopMode::Overdub;
  h.run_frames(600);
  // The loop's C4 starts at 10 of the next round; play C4 over it.
  h.run(32);
  h.note_on(60, 0);
  h.run(4);
  h.note_off(60, 0);
  h.run_frames(640);
  h.node.inputs.mode = LoopMode::Play;
  h.run_frames(640 * 2);
  stop_and_collect(h);
  // The note of the first pass played once, the one replacing it each round.
  CHECK(h.ons().front().at < h.ons()[1].at - 600);
  CHECK(balanced(h.out));
}

TEST_CASE("MIDI looper: quantizing the ends never makes a note vanish",
          "[fx][midi][looper]")
{
  // A sixteenth is 6000 frames. A 400-frame note around a grid point has its
  // start and its end pulled to the same point: it keeps its length.
  midi_harness h{64, 48000};
  h.node.inputs.grid.value = 1.f / 16.f;
  h.node.inputs.strength.value = 1.f;
  h.node.inputs.quantize_ends.value = true;
  h.node.inputs.mode = LoopMode::Record;
  h.run_frames(5760);
  h.note_on(60, 0); // 5760
  h.run_frames(384);
  h.note_off(60, 16); // 6160
  h.run_frames(24000 - 6144);
  h.node.inputs.mode = LoopMode::Play;
  h.run();
  REQUIRE(h.node.loop.length == 24000);

  h.out.clear();
  const int64_t start = h.now; // loop position 64
  h.run_frames(24000);
  const auto ons = h.ons();
  const auto offs = h.offs();
  REQUIRE(ons.size() == 1);
  REQUIRE(offs.size() == 1);
  CHECK(ons[0].at == start + 6000 - 64);
  CHECK(offs[0].at - ons[0].at == 400);
}

TEST_CASE("MIDI looper: full passthrough lets the input through in every mode",
          "[fx][midi][looper][passthrough]")
{
  for(auto mode : {LoopMode::Stop, LoopMode::Play, LoopMode::Record, LoopMode::Overdub})
  {
    midi_harness h;
    h.node.inputs.passthrough = Passthrough::Full;
    h.node.inputs.mode = mode;
    h.run();
    h.out.clear();
    h.note_on(62, 3);
    h.run();
    h.note_off(62, 5);
    h.run();
    INFO(int(mode));
    REQUIRE(h.out.size() == 2);
    CHECK(h.out[0].on);
    CHECK(h.out[0].pitch == 62);
    CHECK(!h.out[1].on);
  }
}

TEST_CASE("MIDI looper: overdubbing an empty loop records it", "[fx][midi][looper]")
{
  midi_harness h;
  h.node.inputs.mode = LoopMode::Overdub;
  h.note_on(60, 10);
  h.run();
  h.note_off(60, 20);
  h.run();
  h.run_frames(512);
  h.node.inputs.mode = LoopMode::Play;
  h.run();
  CHECK(h.node.loop.length == 640);
  CHECK(h.node.loop.note_count == 1);
}

TEST_CASE("MIDI looper: a take can be written as its mix, its passes, or both",
          "[fx][midi][looper][takes]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  midi_harness h;
  auto writer = std::make_shared<Nodes::Looper::take_writer>();
  writer->folder = dir.path();
  writer->prefix = "m";
  writer->extension = "mid";
  h.node.writer = writer;
  h.node.worker.request = [&](Nodes::Looper::midi_take_job job) {
    Looper::worker_t::work(std::move(job))(h.node);
  };
  h.node.inputs.takes.value = Nodes::Looper::TakeFiles::Layers;
  h.node.inputs.restart = Looper::Restart::Recording;

  record(h, 640, [](midi_harness& h) {
    h.note_on(60, 10);
    h.note_off(60, 20);
  });
  h.node.inputs.mode = LoopMode::Overdub;
  h.note_on(67, 10);
  h.note_off(67, 20);
  h.run_frames(640);
  h.node.inputs.mode = LoopMode::Play;
  for(int i = 0; i < 4; i++)
    h.run();

  const auto files = QDir{dir.path()}.entryList({"*.mid"}, QDir::Files, QDir::Name);
  CHECK(!files.contains("m-take2.mid"));
  REQUIRE(files.contains("m-take2-layer1.mid"));
  REQUIRE(files.contains("m-take2-layer2.mid"));

  auto pitches = [&](const char* name) {
    QFile f{dir.path() + "/" + name};
    REQUIRE(f.open(QIODevice::ReadOnly));
    const auto bytes = f.readAll();
    libremidi::reader r;
    std::vector<uint8_t> raw(bytes.begin(), bytes.end());
    r.parse(raw);
    std::vector<int> res;
    for(auto& ev : r.tracks[0])
      if(mtk::classify(ev.m) == mtk::midi_kind::note_on)
        res.push_back(ev.m.bytes[1]);
    return res;
  };
  CHECK(pitches("m-take2-layer1.mid") == std::vector<int>{60});
  CHECK(pitches("m-take2-layer2.mid") == std::vector<int>{67});
}

TEST_CASE("MIDI looper: after a tempo change, the loop keeps its beats, and follows the "
          "tempo when asked",
          "[fx][midi][looper][tempo]")
{
  for(const bool follow : {false, true})
  {
    midi_harness h{64, 48000};
    h.node.inputs.follow_tempo.value = follow;
    h.node.inputs.grid.value = 1.f / 16.f;
    h.node.inputs.strength.value = 1.f;
    h.node.inputs.mode = LoopMode::Record;
    h.run_frames(5900);
    h.note_on(60, 0); // 100 frames before the second sixteenth at 120
    h.run(64);
    h.note_off(60, 0);
    h.run_frames(24000 - 5964);
    h.node.inputs.mode = LoopMode::Play;
    h.run();

    // At 90, a sixteenth would be 8000 frames: quantize stays on the loop's
    // 6000. Following the tempo, the loop goes at 90 / 120 of its speed.
    h.tempo = 90.;
    h.out.clear();
    const int64_t start = h.now; // loop position 64
    h.run_frames(32000);
    const auto ons = h.ons();
    REQUIRE(!ons.empty());
    if(follow)
      CHECK(ons[0].at == start + int64_t((6000 - 64) / 0.75));
    else
      CHECK(ons[0].at == start + 6000 - 64);
  }
}

TEST_CASE("MIDI looper: following the tempo, a loop recorded at 120 lasts its bars at 60",
          "[fx][midi][looper][tempo]")
{
  midi_harness h{64, 48000};
  h.node.inputs.mode = LoopMode::Record;
  h.note_on(60, 0);
  h.run();
  h.note_off(60, 10);
  h.run();
  h.run_frames(48000 - 128); // one bar at 120
  h.node.inputs.mode = LoopMode::Play;
  h.run(); // the recording closes at 120
  h.tempo = 60.;
  h.out.clear();
  h.run_frames(96000 * 3);
  const auto ons = h.ons();
  REQUIRE(ons.size() == 3);
  CHECK(ons[1].at - ons[0].at == 96000);
  CHECK(ons[2].at - ons[1].at == 96000);
}

TEST_CASE("MIDI looper: an extending overdub makes the loop longer, the first pass "
          "repeating under it",
          "[fx][midi][looper]")
{
  midi_harness h;
  h.node.inputs.past_end.value = Nodes::Looper::PastEnd::Extend;
  h.node.inputs.restart = Looper::Restart::Recording;
  record(h, 640, [](midi_harness& h) {
    h.note_on(60, 10);
    h.note_off(60, 20);
  });
  h.node.inputs.mode = LoopMode::Overdub;
  for(int i = 0; i < 40; i++) // four lengths
  {
    if(i == 25) // in the third length
    {
      h.note_on(67, 5);
      h.note_off(67, 15);
    }
    h.run();
  }
  h.node.inputs.mode = LoopMode::Play;
  h.run();
  CHECK(h.node.loop.length == 2560);

  stop_and_collect(h);
  h.out.clear();
  h.node.inputs.mode = LoopMode::Play;
  h.node.stop();
  h.node(h.tick(0));
  h.run_frames(2560);
  int c4 = 0, g4 = 0;
  for(auto& n : h.ons())
    (n.pitch == 60 ? c4 : g4)++;
  CHECK(c4 == 4);
  CHECK(g4 == 1);

  h.press(h.node.inputs.undo);
  CHECK(h.node.loop.length == 640);
}

TEST_CASE("MIDI looper: an extending overdub stopped just past the end does not add a "
          "loop, on the grid and restarting",
          "[fx][midi][looper][musical]")
{
  // The defaults: the loop is read from its start on every change, and the
  // stop waits for the grid -- a bar, which the overdub has just grown the
  // loop by.
  midi_harness h{4800, 48000};
  h.musical = true;
  h.node.inputs.quantif.value = 1.f;
  h.node.inputs.late.value = 120;
  h.node.inputs.past_end.value = Nodes::Looper::PastEnd::Extend;
  const int64_t bar = 96000;

  h.node.inputs.mode = LoopMode::Stop;
  h.run_frames(bar);
  h.node.inputs.mode = LoopMode::Record;
  h.note_on(60, 10);
  h.note_off(60, 20);
  h.run_frames(bar);
  h.node.inputs.mode = LoopMode::Overdub;
  h.run_frames(2 * bar); // the loop grows on each of the two bar lines
  REQUIRE(h.node.loop.length == 3 * bar);

  // 100 ms past the bar line the loop last grew on: the stop is late.
  h.note_on(64, 10); // in the part the stop drops
  h.note_off(64, 20);
  h.run();
  h.node.inputs.mode = LoopMode::Play;
  h.run();

  CHECK(h.node.transport.actualMode == LoopMode::Play);
  CHECK(h.node.loop.length == 2 * bar);
  CHECK(h.node.transport.playbackPos == 2 * h.buffer_size);
  for(int i = 0; i < h.node.loop.note_count; i++)
    CHECK(h.node.loop.notes[i].pitch != 64);
}

TEST_CASE("MIDI looper: an extending overdub cut back to the loop replaces only where "
          "it went",
          "[fx][midi][looper]")
{
  midi_harness h;
  h.node.inputs.past_end.value = Nodes::Looper::PastEnd::Extend;
  h.node.inputs.restart = Looper::Restart::Recording;
  h.node.inputs.overdub_mode = OverdubMode::Replace;
  h.node.inputs.late.value = 50; // 2400 frames

  // A note at 10 and one at 330 of a 640-frame loop.
  h.node.inputs.mode = LoopMode::Record;
  for(int i = 0; i < 10; i++)
  {
    if(i == 0 || i == 5)
    {
      h.note_on(60, 10);
      h.note_off(60, 20);
    }
    h.run();
  }
  h.node.inputs.mode = LoopMode::Play;
  h.run_frames(320);

  // An overdub from the middle of the loop plays the pitch at 350, grows the
  // loop at its end and is stopped a tick later.
  h.node.inputs.mode = LoopMode::Overdub;
  h.note_on(60, 30);
  h.note_off(60, 40);
  h.run_frames(320 + 64);
  h.node.inputs.mode = LoopMode::Play;
  h.run();
  REQUIRE(h.node.loop.length == 640);

  // The pass went over the second half of the loop only.
  int checked = 0;
  for(int i = 0; i < h.node.loop.note_count; i++)
  {
    const auto& n = h.node.loop.notes[i];
    if(n.pass != 0)
      continue;
    INFO(n.start);
    if(n.start == 10)
      CHECK(n.replaced_by == -1);
    else if(n.start == 330)
      CHECK(n.replaced_by == 1);
    checked++;
  }
  CHECK(checked == 2);
}

TEST_CASE("MIDI looper: an overdub at another tempo goes over the frames the loop went "
          "over",
          "[fx][midi][looper][tempo]")
{
  midi_harness h;
  record(h, 48000, [](midi_harness&) {});
  h.run(); // the recording closes at 120

  // 1.3 times as fast: a tick of 64 frames goes over 83.2 frames of the loop.
  h.tempo = 156.;
  h.node.inputs.mode = LoopMode::Overdub;
  for(int i = 0; i < 100; i++)
    h.run();
  CHECK(std::abs(h.node.loop.pass_written - 8320) <= 1);
}

TEST_CASE("MIDI looper: an extending overdub stops growing the loop at an hour",
          "[fx][midi][looper]")
{
  midi_harness h{100, 100}; // an hour is 360000 frames
  h.node.inputs.past_end.value = Nodes::Looper::PastEnd::Extend;
  h.node.inputs.restart = Looper::Restart::Recording;
  record(h, 1000, [](midi_harness& h) {
    h.note_on(60, 10);
    h.note_off(60, 20);
  });
  h.node.inputs.mode = LoopMode::Overdub;
  h.run_frames(400000);
  CHECK(h.node.loop.length == 360000);

  // The first pass plays at each of its repeats, the last ones as well.
  h.node.inputs.mode = LoopMode::Play;
  h.run();
  h.out.clear();
  h.run_frames(360000);
  CHECK(h.ons().size() == 360);
  CHECK(h.offs().size() == 360);
  REQUIRE(!h.ons().empty());
  CHECK(h.ons().back().at - h.ons().front().at == 359000);
}

TEST_CASE("MIDI looper: the files of a take are the loop as it was when it closed",
          "[fx][midi][looper][takes]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  midi_harness h;
  auto writer = std::make_shared<Nodes::Looper::take_writer>();
  writer->folder = dir.path();
  writer->prefix = "m";
  writer->extension = "mid";
  h.node.writer = writer;
  // Written when the test says, as a worker thread would at its own pace.
  std::vector<Nodes::Looper::midi_take_job> jobs;
  h.node.worker.request = [&](Nodes::Looper::midi_take_job job) {
    jobs.push_back(std::move(job));
  };
  auto finish = [&] {
    while(!jobs.empty())
    {
      auto job = std::move(jobs.front());
      jobs.erase(jobs.begin());
      Looper::worker_t::work(std::move(job))(h.node);
    }
  };
  h.node.inputs.takes.value = Nodes::Looper::TakeFiles::Layers;
  h.node.inputs.restart = Looper::Restart::Recording;

  record(h, 640, [](midi_harness& h) {
    h.note_on(60, 10);
    h.note_off(60, 20);
  });
  h.run();
  finish();

  h.node.inputs.mode = LoopMode::Overdub;
  h.note_on(67, 10);
  h.note_off(67, 20);
  h.run_frames(640);
  h.node.inputs.mode = LoopMode::Play;
  h.run();
  // The take of the overdub is being written when a new loop is recorded.
  REQUIRE(!jobs.empty());
  record(h, 640, [](midi_harness& h) {
    h.note_on(72, 10);
    h.note_off(72, 20);
  });
  h.run();
  finish();

  auto pitches = [&](const char* name) {
    QFile f{dir.path() + "/" + name};
    if(!f.open(QIODevice::ReadOnly))
      return std::vector<int>{-1};
    const auto bytes = f.readAll();
    libremidi::reader r;
    std::vector<uint8_t> raw(bytes.begin(), bytes.end());
    r.parse(raw);
    std::vector<int> res;
    for(auto& ev : r.tracks[0])
      if(mtk::classify(ev.m) == mtk::midi_kind::note_on)
        res.push_back(ev.m.bytes[1]);
    return res;
  };
  CHECK(pitches("m-take2-layer1.mid") == std::vector<int>{60});
  CHECK(pitches("m-take2-layer2.mid") == std::vector<int>{67});
}

// ---------------------------------------------------------------------------
// Erasing a pass.
// ---------------------------------------------------------------------------

namespace
{
//! One more overdub pass of `len` frames with the notes `f` plays, then play.
void overdub(midi_harness& h, int64_t len, auto f)
{
  h.node.inputs.mode = LoopMode::Overdub;
  f(h);
  h.run_frames(len);
  h.node.inputs.mode = LoopMode::Play;
  h.run();
}

void erase_layer(midi_harness& h, int layer)
{
  h.node.inputs.layer.value = layer;
  h.press(h.node.inputs.erase_layer);
}

std::multiset<int> round_pitches(midi_harness& h, int64_t len)
{
  h.out.clear();
  h.run_frames(len);
  std::multiset<int> r;
  for(auto& n : h.ons())
    r.insert(n.pitch);
  return r;
}

//! Plays the loop until `pitch` has started, and a little more: the output
//! is then what came since it started.
void sound(midi_harness& h, int pitch)
{
  h.out.clear();
  int64_t started = -1;
  for(int i = 0; i < 40 && started < 0; i++)
  {
    h.run();
    for(auto& n : h.ons())
      if(n.pitch == pitch)
        started = n.at;
  }
  REQUIRE(started >= 0);
  h.run();
  h.run();
  for(auto& n : h.offs())
    REQUIRE((n.pitch != pitch || n.at < started));
  h.out.clear();
}

void short_note(midi_harness& h, int pitch, int at)
{
  h.note_on(pitch, at);
  h.note_off(pitch, at + 5);
}

//! Three passes with Replace: 60 and 64; then 60, 64 and 67, replacing both
//! notes of the first; then 60, replacing the 60 of the first two.
void three_replacing_passes(midi_harness& h)
{
  h.node.inputs.overdub_mode = OverdubMode::Replace;
  h.node.inputs.restart = Looper::Restart::Recording;
  record(h, 640, [](midi_harness& h) {
    short_note(h, 60, 10);
    short_note(h, 64, 30);
  });
  overdub(h, 640, [](midi_harness& h) {
    short_note(h, 60, 10);
    short_note(h, 64, 20);
    short_note(h, 67, 40);
  });
  overdub(h, 640, [](midi_harness& h) { short_note(h, 60, 10); });
}

//! What a view message shows: lane, pitch and whether it is heard.
std::multiset<std::tuple<int, int, bool>> shown(const Nodes::Looper::view_message& v)
{
  std::multiset<std::tuple<int, int, bool>> r;
  for(int i = 0; i < v.notes; i++)
    r.insert({v.note[i].lane, v.note[i].pitch, bool(v.note[i].heard)});
  return r;
}
}

TEST_CASE("MIDI looper: erasing the middle pass brings back what only it replaced",
          "[fx][midi][looper][erase]")
{
  midi_harness h;
  three_replacing_passes(h);
  REQUIRE(h.node.loop.count == 3);
  CHECK(round_pitches(h, 640) == std::multiset<int>{60, 64, 67});

  const int64_t length = h.node.loop.length;
  erase_layer(h, 2);
  CHECK(h.node.loop.count == 2);
  CHECK(h.node.loop.total == 2);
  CHECK(h.node.loop.length == length);
  // Its notes are gone; the 60 of the first pass is still replaced by the
  // last one, and its 64 is heard again.
  CHECK(round_pitches(h, 640) == std::multiset<int>{60, 64});
  for(int i = 0; i < h.node.loop.note_count; i++)
  {
    const auto& n = h.node.loop.notes[i];
    CHECK(n.pass < 2);
    if(n.pass == 0 && n.pitch == 60)
      CHECK(n.replaced_by == 1);
    if(n.pass == 0 && n.pitch == 64)
      CHECK(n.replaced_by == -1);
  }

  // Undo and redo go on with the passes left.
  h.press(h.node.inputs.undo);
  CHECK(round_pitches(h, 640) == std::multiset<int>{60, 64});
  h.press(h.node.inputs.redo);
  CHECK(round_pitches(h, 640) == std::multiset<int>{60, 64});
  erase_layer(h, 2);
  CHECK(round_pitches(h, 640) == std::multiset<int>{60, 64});
}

TEST_CASE("MIDI looper: the view shows a lane per pass, and follows an erase",
          "[fx][midi][looper][erase][view]")
{
  midi_harness h;
  Nodes::Looper::view_message last{};
  h.node.send_message = [&](Nodes::Looper::view_message v) { last = v; };
  three_replacing_passes(h);
  h.run_frames(2000);
  CHECK(last.layers == 3);
  CHECK(last.lanes == 3);
  using shown_t = std::multiset<std::tuple<int, int, bool>>;
  CHECK(
      shown(last)
      == shown_t{
          {0, 60, false},
          {0, 64, false},
          {1, 60, false},
          {1, 64, true},
          {1, 67, true},
          {2, 60, true}});

  erase_layer(h, 2);
  h.run_frames(2000);
  CHECK(last.layers == 2);
  CHECK(last.lanes == 2);
  CHECK(shown(last) == shown_t{{0, 60, false}, {0, 64, true}, {1, 60, true}});
}

TEST_CASE("MIDI looper: past eight passes the oldest share the first lane, and the "
          "notes are shared out between the lanes",
          "[fx][midi][looper][erase][view]")
{
  midi_harness h;
  Nodes::Looper::view_message last{};
  h.node.send_message = [&](Nodes::Looper::view_message v) { last = v; };
  h.node.inputs.restart = Looper::Restart::Recording;
  // A first pass with far more notes than the view holds, then nine with one.
  record(h, 6400, [](midi_harness&) { });
  for(int i = 0; i < 2000; i++)
    h.node.loop.add_note(i * 3, 1, 40 + i % 20, 100)->length = 2;
  for(int p = 0; p < 9; p++)
    overdub(h, 6400, [p](midi_harness& h) { short_note(h, 70 + p, 10); });
  REQUIRE(h.node.loop.count == 10);
  h.run_frames(2000);

  CHECK(last.lanes == 8);
  std::array<int, 8> per_lane{};
  for(int i = 0; i < last.notes; i++)
    per_lane[last.note[i].lane]++;
  // The first lane holds the three oldest passes and gets what the others
  // leave of the message.
  CHECK(per_lane[0] == Nodes::Looper::view_message::max_notes - 7);
  for(int l = 1; l < 8; l++)
    CHECK(per_lane[l] == 1);
}

TEST_CASE("MIDI looper: erasing the pass being overdubbed is undoing it",
          "[fx][midi][looper][erase]")
{
  midi_harness h;
  h.node.inputs.restart = Looper::Restart::Recording;
  record(h, 640, [](midi_harness& h) { short_note(h, 60, 10); });
  h.node.inputs.mode = LoopMode::Overdub;
  short_note(h, 67, 10);
  h.run_frames(128);
  REQUIRE(h.node.loop.count == 2);

  erase_layer(h, 2);
  // The pass was dropped, and overdubbing carries on in a new one.
  CHECK(h.node.loop.count == 2);
  CHECK(h.node.loop.taking);
  for(int i = 0; i < h.node.loop.note_count; i++)
    CHECK(h.node.loop.notes[i].pitch != 67);
  h.node.inputs.mode = LoopMode::Play;
  h.run();
  CHECK(round_pitches(h, 640) == std::multiset<int>{60});
}

TEST_CASE("MIDI looper: erasing the only pass empties the loop and ends its notes",
          "[fx][midi][looper][erase]")
{
  midi_harness h;
  h.node.inputs.restart = Looper::Restart::Recording;
  h.node.inputs.mode = LoopMode::Record;
  h.note_on(60, 10);
  h.run_frames(576);
  h.note_off(60, 0);
  h.run_frames(64);
  h.node.inputs.mode = LoopMode::Play;
  sound(h, 60);

  erase_layer(h, 1);
  CHECK(h.node.loop.empty());
  CHECK(h.offs().size() == 1);
  h.press(h.node.inputs.redo);
  CHECK(h.node.loop.empty());
}

TEST_CASE("MIDI looper: the notes of an erased pass that are sounding end at once",
          "[fx][midi][looper][erase]")
{
  midi_harness h;
  h.node.inputs.restart = Looper::Restart::Recording;
  record(h, 640, [](midi_harness& h) { short_note(h, 60, 10); });
  // A pass with a note held over most of the loop.
  h.node.inputs.mode = LoopMode::Overdub;
  h.note_on(67, 1);
  h.run_frames(576);
  h.note_off(67, 0);
  h.run_frames(64);
  h.node.inputs.mode = LoopMode::Play;
  h.run();
  overdub(h, 640, [](midi_harness& h) { short_note(h, 72, 10); });
  sound(h, 67);

  erase_layer(h, 2);
  CHECK(h.offs().size() == 1);
  CHECK(h.offs()[0].pitch == 67);
  CHECK(round_pitches(h, 640) == std::multiset<int>{60, 72});
}

TEST_CASE("MIDI looper: erasing a pass keeps what redo brings back",
          "[fx][midi][looper][erase]")
{
  midi_harness h;
  h.node.inputs.restart = Looper::Restart::Recording;
  record(h, 640, [](midi_harness& h) { short_note(h, 60, 10); });
  overdub(h, 640, [](midi_harness& h) { short_note(h, 64, 10); });
  overdub(h, 640, [](midi_harness& h) { short_note(h, 67, 10); });
  h.press(h.node.inputs.undo);
  REQUIRE(h.node.loop.count == 2);
  REQUIRE(h.node.loop.total == 3);

  erase_layer(h, 1);
  CHECK(h.node.loop.count == 1);
  CHECK(h.node.loop.total == 2);
  CHECK(round_pitches(h, 640) == std::multiset<int>{64});
  h.press(h.node.inputs.redo);
  CHECK(h.node.loop.count == 2);
  CHECK(round_pitches(h, 640) == std::multiset<int>{64, 67});
}

TEST_CASE("MIDI looper: the view asks for a pass to be erased, a changed loop is left alone",
          "[fx][midi][looper][erase]")
{
  midi_harness h;
  h.node.inputs.restart = Looper::Restart::Recording;
  record(h, 640, [](midi_harness& h) { short_note(h, 60, 10); });
  overdub(h, 640, [](midi_harness& h) { short_note(h, 64, 10); });

  h.node.process_message(Nodes::Looper::erase_request{.pass = 0, .layers = 3});
  h.run();
  CHECK(h.node.loop.count == 2);
  h.node.process_message(Nodes::Looper::erase_request{.pass = 0, .layers = 2});
  h.run();
  CHECK(h.node.loop.count == 1);
  CHECK(round_pitches(h, 640) == std::multiset<int>{64});
}

TEST_CASE("MIDI looper: erasing a pass allocates nothing", "[fx][midi][looper][erase]")
{
  midi_harness h;
  h.pending.reserve(64);
  h.out.reserve(100000);
  h.controls_out.reserve(100000);
  h.node.inputs.overdub_mode = OverdubMode::Replace;
  h.node.inputs.restart = Looper::Restart::Recording;
  h.node.send_message = [](const Nodes::Looper::view_message&) { };

  int allocated = 0;
  {
    alloc_counter::scope counter;
    record(h, 640, [](midi_harness& h) { short_note(h, 60, 10); });
    for(int p = 0; p < 3; p++)
    {
      h.node.inputs.mode = LoopMode::Overdub;
      h.cc(1, p, 5);
      short_note(h, 60 + p, 10);
      h.run_frames(640);
      h.node.inputs.mode = LoopMode::Play;
      h.run();
    }
    h.run_frames(2000);
    erase_layer(h, 2);
    h.node.process_message(Nodes::Looper::erase_request{.pass = 0, .layers = 3});
    h.run();
    h.run_frames(2000);
    erase_layer(h, 1);
    allocated = counter.allocations();
  }
  CHECK(allocated == 0);
}

TEST_CASE("Looper view: the x of a lane asks to erase its pass, and starts no drag",
          "[fx][looper][erase][view]")
{
  Looper::ui ui;
  Looper::ui::bus bus;
  std::vector<Nodes::Looper::erase_request> sent;
  bus.send_message = [&](Nodes::Looper::erase_request r) { sent.push_back(r); };
  bus.init(ui);
  auto& v = ui.view.view;

  Nodes::Looper::view_message m{};
  m.midi = true;
  m.layers = 3;
  m.lanes = 3;
  m.length_seconds = 2.f;
  m.take = 1;
  m.set_take("/tmp/looper-take", "mid", true, 0);
  v.receive(m);

  const auto b = v.erase_box(1);
  const double cx = b.x + b.size / 2., cy = b.y + b.size / 2.;
  // Inside its lane, under the header and its Send button.
  CHECK(b.y >= Nodes::Looper::looper_view::header);
  CHECK(cy > v.lanes_top + v.lane_height());
  CHECK(cy < v.lanes_top + 2. * v.lane_height());
  CHECK(!v.in_send(cx, cy));
  CHECK(v.mouse_press(cx, cy));
  REQUIRE(sent.size() == 1);
  CHECK(sent[0].pass == 1);
  CHECK(sent[0].layers == 3);
  CHECK(v.drag_payload(cx, cy).files.empty());
  // Anywhere else, a press is the start of a drag of the take.
  CHECK(!v.drag_payload(100., 60.).files.empty());
  v.mouse_press(100., 60.);
  CHECK(sent.size() == 1);

  // Ten passes on eight lanes: the first lane holds three, and has no x.
  m.layers = 10;
  m.lanes = 8;
  v.receive(m);
  CHECK(v.pass_of_lane(0) == -1);
  CHECK(v.pass_of_lane(1) == 3);
  CHECK(v.pass_of_lane(7) == 9);
  const auto b0 = v.erase_box(0);
  v.mouse_press(b0.x + b0.size / 2., b0.y + b0.size / 2.);
  CHECK(sent.size() == 1);
  const auto b7 = v.erase_box(7);
  v.mouse_press(b7.x + b7.size / 2., b7.y + b7.size / 2.);
  REQUIRE(sent.size() == 2);
  CHECK(sent[1].pass == 9);
  CHECK(sent[1].layers == 10);

  // An empty loop has nothing to erase.
  m.empty = true;
  v.receive(m);
  v.mouse_press(b7.x + b7.size / 2., b7.y + b7.size / 2.);
  CHECK(sent.size() == 2);
}
