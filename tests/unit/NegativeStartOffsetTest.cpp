// A negative start offset delays a process's content: a process whose offset
// is -d plays nothing for d, then its content from its beginning. A one-shot
// recording placed in a box after the box's start is such a process.
//
// - token_request::clip_before_zero keeps the part of a tick at or after 0,
//   with its place in the tick and its share of the sample span;
// - looping_process runs its state only from date 0, looping or not;
// - the sound nodes that read files (mmap, libav streaming) read silence
//   before the file's first frame, and apply the start offset at all (the
//   libav streaming node used to ignore it).
//
// The sound fixture is a ramp, sample[i] == i / frames, so a sample read back
// names the file frame it came from.

#include <ossia/audio/drwav_handle.hpp>
#include <ossia/dataflow/nodes/sound_libav.hpp>
#include <ossia/dataflow/nodes/sound_mmap.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/libav.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/editor/scenario/time_process.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <cstdint>
#include <vector>

using Catch::Approx;
using ossia::time_value;

namespace
{
constexpr int rate = 44100;
constexpr int64_t frames = int64_t(rate) * 2;

void u32(QByteArray& o, quint32 v)
{
  for(int i = 0; i < 4; i++)
    o.append(char((v >> (8 * i)) & 0xff));
}
void u16(QByteArray& o, quint16 v)
{
  for(int i = 0; i < 2; i++)
    o.append(char((v >> (8 * i)) & 0xff));
}

//! 32-bit float mono, sample[i] = (i + 1) / frames: no frame reads as 0, so a
//! 0 read back is silence.
QByteArray ramp()
{
  const quint32 data_size = quint32(frames * 4);
  QByteArray out;
  out.append("RIFF");
  u32(out, 36 + data_size);
  out.append("WAVE");
  out.append("fmt ");
  u32(out, 16);
  u16(out, 3); // IEEE float
  u16(out, 1);
  u32(out, rate);
  u32(out, rate * 4);
  u16(out, 4);
  u16(out, 32);
  out.append("data");
  u32(out, data_size);
  for(int64_t i = 0; i < frames; i++)
  {
    const float v = float(double(i + 1) / double(frames));
    out.append(reinterpret_cast<const char*>(&v), 4);
  }
  return out;
}

//! The file frame a sample value came from, or -1 for silence.
int64_t frame_of(double value)
{
  if(value == 0.)
    return -1;
  return std::llround(value * double(frames)) - 1;
}

//! What reading `count` frames from `start` should give: the file frame at
//! each position, -1 where it is silence.
std::vector<int64_t>
expected(int64_t start, int64_t count, int64_t offset, int64_t loop = 0)
{
  std::vector<int64_t> res;
  for(int64_t k = 0; k < count; k++)
  {
    int64_t pos = start + k;
    if(loop > 0)
      pos = ((pos % loop) + loop) % loop;
    pos += offset;
    res.push_back(pos >= 0 && pos < frames ? pos : -1);
  }
  return res;
}

template <typename Read>
std::vector<int64_t> read_frames(Read&& read, int64_t start, int64_t count)
{
  std::vector<double> buf(count, 12345.);
  double* out[1] = {buf.data()};
  read(start, count, out);
  std::vector<int64_t> res;
  for(double v : buf)
    res.push_back(frame_of(v));
  return res;
}

//! A process that records the requests its state gets.
struct recording_process final : ossia::looping_process<recording_process>
{
  std::vector<ossia::token_request> seen;
  void state_impl(const ossia::token_request& tk) { seen.push_back(tk); }
  void offset_impl(time_value) override { }
  void transport_impl(time_value) override { }
};

ossia::token_request tick(int64_t from, int64_t to)
{
  ossia::token_request tk;
  tk.prev_date = time_value{from};
  tk.date = time_value{to};
  tk.start_sample = 0;
  tk.length_sample = int32_t(std::abs(to - from));
  return tk;
}
}

TEST_CASE("clip_before_zero keeps the part of a tick at or after 0", "[ossia][offset]")
{
  ossia::token_request out;

  SECTION("after 0: unchanged")
  {
    const auto tk = tick(100, 300);
    REQUIRE(tk.clip_before_zero(out));
    CHECK(out.prev_date == tk.prev_date);
    CHECK(out.date == tk.date);
    CHECK(out.offset == tk.offset);
    CHECK(out.start_sample == 0);
    CHECK(out.length_sample == 200);
  }

  SECTION("before 0: nothing")
  {
    CHECK(!tick(-300, -100).clip_before_zero(out));
    CHECK(!tick(-300, 0).clip_before_zero(out));
  }

  SECTION("across 0: from 0, later in the tick, with the rest of the span")
  {
    const auto tk = tick(-50, 150);
    REQUIRE(tk.clip_before_zero(out));
    CHECK(out.prev_date == time_value{0});
    CHECK(out.date == time_value{150});
    CHECK(out.offset == time_value{50});
    CHECK(out.start_sample == 50);
    CHECK(out.length_sample == 150);
  }

  SECTION("a tick that does not advance, at 0 or after")
  {
    CHECK(tick(0, 0).clip_before_zero(out));
    CHECK(tick(10, 10).clip_before_zero(out));
    CHECK(!tick(-10, -10).clip_before_zero(out));
  }

  SECTION("backward across 0: down to 0, with the first part of the span")
  {
    const auto tk = tick(60, -40);
    REQUIRE(tk.clip_before_zero(out));
    CHECK(out.prev_date == time_value{60});
    CHECK(out.date == time_value{0});
    CHECK(out.offset == time_value{0});
    CHECK(out.start_sample == 0);
    CHECK(out.length_sample == 60);
    CHECK(!tick(-10, -40).clip_before_zero(out));
  }
}

TEST_CASE(
    "A process with a negative start offset runs from its date 0", "[ossia][offset]")
{
  recording_process p;

  SECTION("not looping")
  {
    p.set_start_offset(time_value{-100});
    p.state(tick(0, 60));
    CHECK(p.seen.empty());
    p.state(tick(60, 250));
    REQUIRE(p.seen.size() == 1);
    CHECK(p.seen[0].prev_date == time_value{0});
    CHECK(p.seen[0].date == time_value{150});
    CHECK(p.seen[0].offset == time_value{40});
    CHECK(p.seen[0].start_sample == 40);
    CHECK(p.seen[0].length_sample == 150);
  }

  SECTION("looping: every loop starts with the delay")
  {
    p.set_loops(true);
    p.set_loop_duration(time_value{1000});
    p.set_start_offset(time_value{-200});
    // [900; 1100[ is [700; 800[ of the first loop, then [-200; -100[ of the
    // second, which is before its content.
    p.state(tick(900, 1100));
    REQUIRE(p.seen.size() == 1);
    CHECK(p.seen[0].prev_date == time_value{700});
    CHECK(p.seen[0].date == time_value{800});
    // [1150; 1300[ is [-50; 100[ of the second loop: from 0, 50 into the tick.
    p.seen.clear();
    p.state(tick(1150, 1300));
    REQUIRE(p.seen.size() == 1);
    CHECK(p.seen[0].prev_date == time_value{0});
    CHECK(p.seen[0].date == time_value{100});
    CHECK(p.seen[0].offset == time_value{50});
  }

  SECTION("a positive offset is unchanged")
  {
    p.set_start_offset(time_value{100});
    p.state(tick(0, 60));
    REQUIRE(p.seen.size() == 1);
    CHECK(p.seen[0].prev_date == time_value{100});
    CHECK(p.seen[0].date == time_value{160});
  }
}

TEST_CASE("The mmap sound node reads silence before its file", "[sound][offset]")
{
  const QByteArray wav = ramp();
  ossia::nodes::sound_mmap snd;
  snd.set_sound(ossia::drwav_handle{wav.constData(), std::size_t(wav.size())});
  REQUIRE(snd.channels() == 1);
  auto read
      = [&](int64_t start, int64_t n, double** out) { snd.fetch_audio(start, n, out); };

  for(const int64_t offset : {int64_t(-1000), int64_t(0), int64_t(500)})
  {
    INFO("start offset " << offset);
    snd.m_loops = false;
    snd.m_start_offset_samples = offset;
    for(const int64_t start : {int64_t(0), int64_t(900), int64_t(1100), int64_t(-300)})
    {
      INFO("start " << start);
      CHECK(read_frames(read, start, 256) == expected(start, 256, offset));
    }
  }

  SECTION("looping")
  {
    snd.m_loops = true;
    snd.m_loop_duration_samples = 1000;
    snd.m_start_offset_samples = -200;
    for(const int64_t start : {int64_t(0), int64_t(900), int64_t(1150)})
    {
      INFO("start " << start);
      CHECK(read_frames(read, start, 256) == expected(start, 256, -200, 1000));
    }
  }
}

TEST_CASE(
    "The libav streaming sound node applies the start offset", "[sound][offset][libav]")
{
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.path() + "/ramp.wav";
  {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    const QByteArray wav = ramp();
    REQUIRE(f.write(wav) == wav.size());
  }

  ossia::libav_handle h;
  h.open(path.toStdString(), 0, rate);
  REQUIRE(bool(h));
  ossia::nodes::sound_libav snd;
  snd.set_sound(std::move(h));
  auto read
      = [&](int64_t start, int64_t n, double** out) { snd.fetch_audio(start, n, out); };

  for(const int64_t offset : {int64_t(-1000), int64_t(0), int64_t(500)})
  {
    INFO("start offset " << offset);
    snd.m_loops = false;
    snd.m_start_offset_samples = offset;
    for(const int64_t start : {int64_t(0), int64_t(900), int64_t(1100), int64_t(-300)})
    {
      INFO("start " << start);
      CHECK(read_frames(read, start, 256) == expected(start, 256, offset));
    }
  }

  SECTION("looping")
  {
    snd.m_loops = true;
    snd.m_loop_duration_samples = 1000;
    snd.m_start_offset_samples = -200;
    for(const int64_t start : {int64_t(0), int64_t(900), int64_t(1150)})
    {
      INFO("start " << start);
      CHECK(read_frames(read, start, 256) == expected(start, 256, -200, 1000));
    }
  }

  SECTION("backward, not looping")
  {
    snd.m_loops = false;
    snd.m_start_offset_samples = -100;
    std::vector<double> buf(64, 12345.);
    double* out[1] = {buf.data()};
    // Newest sample at 120: positions 120 down to 57, file frames 20 down to
    // -43, i.e. silence after the first 21.
    snd.fetch_audio_backward(120, 64, out);
    for(int k = 0; k < 64; k++)
    {
      INFO("k " << k);
      const int64_t want = 120 - k - 100;
      CHECK(frame_of(buf[k]) == (want >= 0 ? want : -1));
    }
  }
}

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/midi.hpp>

namespace
{
//! Runs what a process requested of its MIDI node, and gives the note-ons:
//! their dates in the tick (timestamps), in order.
std::vector<int64_t> run_midi(ossia::nodes::midi& node, ossia::exec_state_facade fac)
{
  std::vector<int64_t> ons;
  auto& mp = *node.root_outputs()[0]->target<ossia::midi_port>();
  ossia::graph_node& base = node;
  const auto tokens = node.requested_tokens;
  node.requested_tokens.clear();
  for(const auto& tk : tokens)
  {
    mp.messages.clear();
    base.run(tk, fac);
    for(const auto& m : mp.messages)
      if(cmidi2_ump_get_status_code(&m.data[0]) == CMIDI2_STATUS_NOTE_ON)
        ons.push_back(int64_t(m.timestamp)); // includes the token's offset
  }
  return ons;
}

ossia::token_request box_tick(int64_t from, int64_t to)
{
  return ossia::token_request{
      time_value{from},
      time_value{to},
      time_value{100000},
      time_value{0},
      1.,
      ossia::time_signature{4, 4},
      120.};
}
}

TEST_CASE("A MIDI process joins a playing box in step", "[midi][offset]")
{
  ossia::execution_state st;
  ossia::exec_state_facade fac{&st};

  SECTION("looping, added mid-loop: the notes still to come in this loop play")
  {
    auto node = std::make_shared<ossia::nodes::midi>(8);
    node->add_note({time_value{800}, time_value{50}, 60, 100});
    ossia::nodes::midi_node_process p{node};
    p.set_loops(true);
    p.set_loop_duration(time_value{1000});

    // Added while its box is at 2600: the interval transports it there.
    p.transport(time_value{2600});
    p.state(box_tick(2600, 2750));
    CHECK(run_midi(*node, fac).empty());
    // Content [750; 850[: the note at 800 starts 50 into the tick.
    p.state(box_tick(2750, 2850));
    CHECK(run_midi(*node, fac) == std::vector<int64_t>{50});
    // And again in the next loop, at 3800.
    p.state(box_tick(2850, 3700));
    CHECK(run_midi(*node, fac).empty());
    p.state(box_tick(3700, 3900));
    CHECK(run_midi(*node, fac) == std::vector<int64_t>{100});
  }

  SECTION("a negative start offset delays the notes")
  {
    auto node = std::make_shared<ossia::nodes::midi>(8);
    node->add_note({time_value{100}, time_value{50}, 60, 100});
    ossia::nodes::midi_node_process p{node};
    p.set_start_offset(time_value{-500});
    p.state(box_tick(0, 450));
    CHECK(run_midi(*node, fac).empty());
    // Box [450; 650[ is content [-50; 150[: run from content 0, 50 into the
    // tick, so the note at 100 is 150 into it.
    p.state(box_tick(450, 650));
    CHECK(run_midi(*node, fac) == std::vector<int64_t>{150});
  }
}
