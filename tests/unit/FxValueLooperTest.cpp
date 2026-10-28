#include <Fx/Looper/ValueLooper.hpp>

#include <ossia/dataflow/execution_state.hpp>

#include "AllocCounter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

template <>
struct Catch::StringMaker<ossia::value>
{
  static std::string convert(const ossia::value& v) { return ossia::value_to_pretty_string(v); }
};

namespace
{
using Looper = Nodes::ValueLooper::Node;
using LoopMode = Looper::LoopMode;
using Passthrough = Looper::Passthrough;

struct sent
{
  int64_t at; //!< frame on the harness' count
  ossia::value value;
};

//! Drives the value looper like the ossia binding does: one tick per buffer,
//! the ports' timestamps from the start of the buffer.
struct value_harness
{
  std::unique_ptr<Looper> owner{std::make_unique<Looper>()};
  Looper& node{*owner};
  ossia::execution_state st;
  ossia::value_port in;
  ossia::value_port out_port;
  int buffer_size{100};
  int64_t now{0};
  double tempo{120.};
  //! Ticks carry their place in the bars, 4/4 from the start, for the grid.
  bool musical{false};
  std::vector<ossia::timed_value> pending;
  std::vector<sent> out;

  explicit value_harness(int bs = 100, int rate = 48000)
      : buffer_size{bs}
  {
    st.sampleRate = rate;
    st.bufferSize = bs;
    st.modelToSamplesRatio = 1.;
    st.samplesToModelRatio = 1.;
    node.ossia_state = {&st};
    node.inputs.values.value = &in;
    node.outputs.values.value = &out_port;
    node.inputs.quantif.value = halp::grid_value{};
    node.inputs.late.value = 0;
    node.inputs.passthrough = Passthrough::None;
    pending.reserve(64);
    in.get_data().reserve(64);
    out_port.get_data().reserve(1024);
    out.reserve(100000);
  }

  void send(ossia::value v, int at = 0) { pending.emplace_back(std::move(v), at); }

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
      st.samples_since_start = now + frames;
    }
    return tk;
  }

  void run(int64_t frames = -1)
  {
    if(frames < 0)
      frames = buffer_size;
    in.clear();
    for(auto& p : pending)
      in.write_value(p.value, p.timestamp);
    pending.clear();
    out_port.clear();
    node(tick(frames));
    for(auto& v : out_port.get_data())
      out.push_back({now + v.timestamp, v.value});
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

  void press(auto& button)
  {
    button.value.emplace();
    run();
    button.value.reset();
  }

  //! What went out from frame `from` on.
  std::vector<sent> since(int64_t from) const
  {
    std::vector<sent> r;
    for(auto& s : out)
      if(s.at >= from)
        r.push_back(s);
    return r;
  }

  //! The value of each event of the loop, by position.
  std::vector<std::pair<int64_t, ossia::value>> events(int pass = -1) const
  {
    std::vector<std::pair<int64_t, ossia::value>> r;
    for(int i = 0; i < node.loop.event_count; i++)
      if(pass < 0 || node.loop.events[i].pass == pass)
        r.emplace_back(node.loop.events[i].pos, node.loop.events[i].value.get());
    std::stable_sort(r.begin(), r.end(), [](auto& a, auto& b) { return a.first < b.first; });
    return r;
  }
};

//! Records a loop of `len` frames with the values `f` sends, buffer by buffer
//! (`f(h, buffer)`), then plays it.
void record(value_harness& h, int64_t len, auto f)
{
  h.node.inputs.mode = LoopMode::Record;
  for(int b = 0; b * h.buffer_size < len; b++)
  {
    f(h, b);
    h.run();
  }
  h.node.inputs.mode = LoopMode::Play;
}

bool same(const ossia::value& a, const ossia::value& b)
{
  return Nodes::Looper::value_slot::same(a, b);
}

//! The values that went out over [from; to[, without their frames.
std::vector<ossia::value> values_between(const value_harness& h, int64_t from, int64_t to)
{
  std::vector<ossia::value> r;
  for(auto& s : h.out)
    if(s.at >= from && s.at < to)
      r.push_back(s.value);
  return r;
}

//! The frames of what went out over [from; to[, from `from`.
std::vector<int64_t> frames_between(const value_harness& h, int64_t from, int64_t to)
{
  std::vector<int64_t> r;
  for(auto& s : h.out)
    if(s.at >= from && s.at < to)
      r.push_back(s.at - from);
  return r;
}

//! A loop of 4800 frames holding 1 from 0, 2 from 1200, 3 from 2400 and 4
//! from 3600, played from its start.
void four_steps(value_harness& h)
{
  record(h, 4800, [](value_harness& h, int b) {
    if(b == 0)
      h.send(1.f, 0);
    if(b == 12)
      h.send(2.f, 0);
    if(b == 24)
      h.send(3.f, 0);
    if(b == 36)
      h.send(4.f, 0);
  });
  REQUIRE(h.node.loop.length == 4800);
}
}

TEST_CASE("Value looper: a value is held until the next one, round the loop",
          "[fx][values][looper]")
{
  value_harness h;
  record(h, 640, [](value_harness& h, int b) {
    if(b == 0)
      h.send(0.1f, 10);
    if(b == 3)
      h.send(0.5f, 0);
  });
  REQUIRE(h.node.loop.length == 700);
  // The loop starts on the value held at its start: the last one, round the
  // loop.
  const int64_t start = h.now;
  h.run_frames(3 * 700);
  const auto v = values_between(h, start, start + 3 * 700);
  const auto f = frames_between(h, start, start + 3 * 700);
  CAPTURE(v, f);
  REQUIRE(v.size() == 7);
  for(int i = 0; i < 7; i++)
    CHECK(same(v[i], ossia::value{i % 2 == 0 ? 0.5f : 0.1f}));
  CHECK(f == std::vector<int64_t>{0, 10, 300, 710, 1000, 1410, 1700});
}

TEST_CASE("Value looper: a recording starts with the value held when it starts",
          "[fx][values][looper]")
{
  value_harness h;
  h.send(0.3f, 50);
  h.run();
  // Nothing moves while it is recorded.
  record(h, 1000, [](value_harness&, int) { });
  const auto e = h.events();
  REQUIRE(e.size() == 1);
  CHECK(e[0].first == 0);
  CHECK(same(e[0].second, 0.3f));
  const int64_t start = h.now;
  h.run_frames(2000);
  CHECK(values_between(h, start, start + 2000).size() == 1);
  CHECK(same(h.since(start).at(0).value, 0.3f));
}

TEST_CASE("Value looper: only a new value is recorded, the last of a frame",
          "[fx][values][looper]")
{
  value_harness h;
  record(h, 400, [](value_harness& h, int b) {
    if(b == 0)
    {
      h.send(0.5f, 10);
      h.send(0.5f, 20);
      h.send(0.7f, 30);
      h.send(0.8f, 30);
    }
    if(b == 1)
    {
      // Equal lists are the same value; an int is not a float.
      h.send(std::vector<ossia::value>{1, std::string{"a"}}, 0);
      h.send(std::vector<ossia::value>{1, std::string{"a"}}, 10);
      h.send(1, 20);
      h.send(1.f, 30);
      // Impulses all count, but one a frame.
      h.send(ossia::impulse{}, 40);
      h.send(ossia::impulse{}, 40);
      h.send(ossia::impulse{}, 50);
    }
  });
  const auto e = h.events();
  REQUIRE(e.size() == 7);
  CHECK(e[0].first == 10);
  CHECK(same(e[0].second, 0.5f));
  CHECK(e[1].first == 30);
  CHECK(same(e[1].second, 0.8f));
  CHECK(e[2].first == 100);
  CHECK(e[3].first == 120);
  CHECK(same(e[3].second, 1));
  CHECK(e[4].first == 130);
  CHECK(same(e[4].second, 1.f));
  CHECK(e[5].first == 140);
  CHECK(e[6].first == 150);
}

TEST_CASE("Value looper: lists and maps come back as they went in", "[fx][values][looper]")
{
  value_harness h;
  const ossia::value list
      = std::vector<ossia::value>{1, std::string{"two"}, std::vector<ossia::value>{2.5f, true}};
  ossia::value_map_type map;
  map.emplace_back("x", ossia::value{1.5f});
  map.emplace_back("y", ossia::value{std::vector<ossia::value>{ossia::vec3f{1, 2, 3}, 4}});
  const ossia::value as_map{map};
  const ossia::value text{std::string{"hello"}};
  record(h, 1000, [&](value_harness& h, int b) {
    if(b == 1)
      h.send(list, 0);
    if(b == 4)
      h.send(as_map, 0);
    if(b == 7)
      h.send(text, 0);
  });
  const int64_t start = h.now;
  h.run_frames(1000);
  const auto v = values_between(h, start, start + 1000);
  // The text held round the loop's start, then each in turn.
  REQUIRE(v.size() == 4);
  CHECK(same(v[0], text));
  CHECK(same(v[1], list));
  CHECK(v[1].get<std::vector<ossia::value>>()[1].get<std::string>() == "two");
  CHECK(same(v[2], as_map));
  CHECK(same(v[3], text));
}

TEST_CASE("Value looper: impulses fire where they came, every pass heard",
          "[fx][values][looper]")
{
  value_harness h;
  record(h, 4800, [](value_harness& h, int b) {
    if(b == 1)
      h.send(ossia::impulse{}, 0);
    if(b == 20)
      h.send(ossia::impulse{}, 0);
  });
  // An overdub from the loop's start adds one between them.
  h.node.inputs.mode = LoopMode::Overdub;
  for(int b = 0; b < 48; b++)
  {
    if(b == 10)
      h.send(ossia::impulse{}, 0);
    h.run();
  }
  h.node.inputs.mode = LoopMode::Play;
  REQUIRE(h.node.loop.count == 2);
  const int64_t start = h.now;
  h.run_frames(4800);
  CHECK(frames_between(h, start, start + 4800) == std::vector<int64_t>{100, 1000, 2000});
  for(auto& v : values_between(h, start, start + 4800))
    CHECK(v.get_type() == ossia::val_type::IMPULSE);
}

TEST_CASE("Value looper: an overdub holds a value from its first new one to its end",
          "[fx][values][looper][latch]")
{
  value_harness h;
  four_steps(h);
  REQUIRE(h.node.transport.playbackPos % 4800 == 0);
  // An overdub from the start: nothing new until 1800, then 10, 11 at 2600,
  // and it ends at 3000.
  h.node.inputs.mode = LoopMode::Overdub;
  for(int b = 0; b < 30; b++)
  {
    if(b == 18)
      h.send(10.f, 0);
    if(b == 26)
      h.send(11.f, 0);
    h.run();
  }
  h.node.inputs.mode = LoopMode::Play;
  // Played from the end of the pass: the older layers come back at once.
  const int64_t end = h.now;
  h.run();
  CHECK(values_between(h, end, end + 100) == std::vector<ossia::value>{3.f});

  h.run_frames(1700);
  REQUIRE(h.node.transport.playbackPos % 4800 == 0);
  const int64_t start = h.now;
  h.run_frames(4800);
  // Before its first value, the layer under it; after its end, the layer
  // under it again, from what it holds there.
  const auto v = values_between(h, start, start + 4800);
  CHECK(v == std::vector<ossia::value>{1.f, 2.f, 10.f, 11.f, 3.f, 4.f});
  CHECK(
      frames_between(h, start, start + 4800)
      == std::vector<int64_t>{0, 1200, 1800, 2600, 3000, 3600});
}

TEST_CASE("Value looper: a pass going round again writes over its last round",
          "[fx][values][looper][latch]")
{
  value_harness h;
  four_steps(h);
  h.node.inputs.mode = LoopMode::Overdub;
  for(int b = 0; b < 48 + 24; b++)
  {
    // The first round moves from 600; the second sets 20 at 300, and
    // nothing after.
    if(b == 6)
      h.send(10.f, 0);
    if(b == 30)
      h.send(11.f, 0);
    if(b == 48 + 3)
      h.send(20.f, 0);
    h.run();
  }
  h.node.inputs.mode = LoopMode::Play;
  h.run_frames(2400);
  const int64_t start = h.now;
  h.run_frames(4800);
  // 20 from 300, where the second round went: the 10 the first round held
  // from 600 is written over; the 11 it held from 3000, past where the
  // second round ended, is not.
  CHECK(values_between(h, start, start + 4800) == std::vector<ossia::value>{20.f, 11.f});
  CHECK(frames_between(h, start, start + 4800) == std::vector<int64_t>{300, 3000});
}

TEST_CASE("Value looper: muting, erasing, undoing a layer sends what is held then",
          "[fx][values][looper][latch]")
{
  value_harness h;
  four_steps(h);
  h.node.inputs.mode = LoopMode::Overdub;
  for(int b = 0; b < 30; b++)
  {
    if(b == 18)
      h.send(10.f, 0);
    h.run();
  }
  h.node.inputs.mode = LoopMode::Play;
  // Into what the overdub holds, five ticks before the recording's 3 at 2400.
  h.run_frames(4800 - 3000 + 1900);
  REQUIRE(h.node.transport.playbackPos % 4800 == 1900);
  auto& in = h.node.inputs;
  auto last = [&] { return h.out.back().value; };
  REQUIRE(same(last(), 10.f));

  in.layer.value = 2;
  int64_t at = h.now;
  h.press(in.mute_layer);
  CHECK(values_between(h, at, h.now) == std::vector<ossia::value>{2.f});
  at = h.now;
  h.press(in.mute_layer);
  CHECK(values_between(h, at, h.now) == std::vector<ossia::value>{10.f});
  at = h.now;
  h.press(in.undo);
  CHECK(values_between(h, at, h.now) == std::vector<ossia::value>{2.f});
  at = h.now;
  h.press(in.redo);
  CHECK(values_between(h, at, h.now) == std::vector<ossia::value>{10.f});
  at = h.now;
  in.layer.value = 2;
  h.press(in.erase_layer);
  CHECK(h.node.loop.count == 1);
  CHECK(values_between(h, at, h.now) == std::vector<ossia::value>{2.f});
  // Then what the recording holds from 2400, and nothing else.
  at = h.now;
  h.run_frames(500);
  CHECK(values_between(h, at, h.now) == std::vector<ossia::value>{3.f});
  CHECK(frames_between(h, at, h.now) == std::vector<int64_t>{0});
}

TEST_CASE("Value looper: Play sends the value held where the loop starts again",
          "[fx][values][looper]")
{
  value_harness h;
  four_steps(h);
  h.run_frames(1300);
  h.node.inputs.mode = LoopMode::Stop;
  h.run_frames(1000);
  // Retriggered from the start: 1, not 2.
  h.node.inputs.start_from = Nodes::Looper::StartFrom::LoopStart;
  h.node.inputs.mode = LoopMode::Play;
  const int64_t at = h.now;
  h.run();
  CHECK(values_between(h, at, h.now) == std::vector<ossia::value>{1.f});
  // Stopped and played again where it was: nothing to send.
  h.node.inputs.start_from = Nodes::Looper::StartFrom::WhereItWas;
  h.node.inputs.mode = LoopMode::Stop;
  h.run();
  h.node.inputs.mode = LoopMode::Play;
  const int64_t again = h.now;
  h.run();
  CHECK(values_between(h, again, h.now).empty());
}

TEST_CASE("Value looper: played backwards, a value goes back to the one before it",
          "[fx][values][looper][reverse]")
{
  value_harness h;
  record(h, 3600, [](value_harness& h, int b) {
    if(b == 0)
      h.send(1.f, 0);
    if(b == 12)
      h.send(2.f, 0);
    if(b == 24)
      h.send(3.f, 0);
  });
  h.node.inputs.reverse.value = true;
  h.run_frames(3600);
  const int64_t start = h.now;
  h.run_frames(3600);
  // From the loop's start going backwards: 3 held at its end, then 2 below
  // 2400, 1 below 1200.
  const auto v = values_between(h, start, start + 3600);
  CHECK(v == std::vector<ossia::value>{3.f, 2.f, 1.f});
  CHECK(frames_between(h, start, start + 3600) == std::vector<int64_t>{0, 1200, 2400});
}

TEST_CASE("Value looper: at double speed, the values come twice as often",
          "[fx][values][looper][speed]")
{
  value_harness h;
  record(h, 4800, [](value_harness& h, int b) {
    if(b == 12)
      h.send(1.f, 0);
    if(b == 36)
      h.send(2.f, 0);
  });
  h.node.inputs.speed.value = 2.f;
  h.run_frames(2400);
  const int64_t start = h.now;
  h.run_frames(4800);
  CHECK(
      frames_between(h, start, start + 4800) == std::vector<int64_t>{600, 1800, 3000, 4200});
}

TEST_CASE("Value looper: Capture loops what came in, from the value held before it",
          "[fx][values][looper][capture]")
{
  value_harness h;
  // One bar at 120 is 96000 frames.
  h.node.inputs.capture_bars.value = 1;
  h.send(0.1f, 0);
  h.run();
  h.run_frames(20000);
  h.send(0.2f, 0);
  h.run_frames(50000);
  h.send(0.3f, 0);
  h.run_frames(30000);
  const int64_t now = h.now;
  h.press(h.node.inputs.capture);
  REQUIRE(h.node.loop.length == 96000);
  CHECK(h.node.transport.actualMode == LoopMode::Play);
  const auto e = h.events();
  REQUIRE(e.size() == 3);
  CHECK(e[0].first == 0);
  CHECK(same(e[0].second, 0.1f));
  CHECK(e[1].first == 20100 - (now - 96000));
  CHECK(same(e[1].second, 0.2f));
  CHECK(e[2].first == 70100 - (now - 96000));
  CHECK(same(e[2].second, 0.3f));
}

TEST_CASE("Value looper: Passthrough Always lets the input win while it moves",
          "[fx][values][looper][passthrough]")
{
  value_harness h;
  four_steps(h);
  h.node.inputs.passthrough = Passthrough::Full;
  h.run_frames(200);
  // The input moves around 1200, where the loop has 2.
  for(int b = 0; b < 12; b++)
  {
    h.send(float(100 + b), 50);
    h.run();
  }
  const int64_t moved = h.now;
  // 0.2 s later, the loop takes over with what it holds.
  h.run_frames(9600 + 200);
  const auto v = values_between(h, moved, h.now);
  REQUIRE(!v.empty());
  CHECK(same(v.front(), 2.f));
  CHECK(frames_between(h, moved, h.now).front() >= 9600 - 100 - 50);
}

TEST_CASE("Value looper: numbers and vectors allocate nothing", "[fx][values][looper]")
{
  value_harness h;
  h.node.inputs.passthrough = Passthrough::Full;
  int allocated = -1;
  {
    alloc_counter::scope counter;
    h.node.inputs.mode = LoopMode::Record;
    for(int i = 0; i < 40; i++)
    {
      h.send(float(i), 3);
      h.send(i, 7);
      h.send(ossia::vec3f{float(i), 0.f, 1.f}, 9);
      h.send(ossia::impulse{}, 11);
      h.send(i % 2 == 0, 13);
      h.run();
    }
    h.node.inputs.mode = LoopMode::Overdub;
    for(int i = 0; i < 80; i++)
    {
      if(i % 3 == 0)
        h.send(float(-i), 5);
      h.run();
    }
    h.node.inputs.reverse.value = true;
    h.node.inputs.mode = LoopMode::Play;
    for(int i = 0; i < 40; i++)
      h.run();
    h.node.inputs.reverse.value = false;
    h.node.inputs.layer.value = 1;
    h.press(h.node.inputs.mute_layer);
    h.press(h.node.inputs.undo);
    h.press(h.node.inputs.redo);
    h.press(h.node.inputs.capture);
    h.press(h.node.inputs.clear);
    allocated = counter.allocations();
  }
  CHECK(allocated == 0);
}

namespace
{
std::shared_ptr<Nodes::Looper::take_writer> temp_writer(const QTemporaryDir& dir)
{
  auto writer = std::make_shared<Nodes::Looper::take_writer>();
  writer->folder = dir.path();
  writer->prefix = "v";
  writer->extension = "values";
  return writer;
}

//! Records a loop of what `f` sends, then writes the take its pass closed
//! with: what the view was last told of it, and the file.
struct take_result
{
  bool placeable{};
  QJsonObject file;
};
take_result written_take(auto f)
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  value_harness h;
  h.node.writer = temp_writer(dir);
  std::vector<Nodes::Looper::value_take_job> jobs;
  h.node.worker.request = [&](Nodes::Looper::value_take_job job) {
    jobs.push_back(std::move(job));
  };
  take_result r;
  h.node.send_message = [&](const Nodes::Looper::view_message& m) {
    if(m.take > 0)
      r.placeable = m.take_placeable;
  };
  record(h, 4800, f);
  h.run();
  REQUIRE(jobs.size() == 1);
  Looper::worker_t::work(std::move(jobs[0]))(h.node);
  h.run_frames(4800);
  const auto files = QDir{dir.path()}.entryList({"*.values"}, QDir::Files, QDir::Name);
  REQUIRE(files == QStringList{"v-take1.values"});
  QFile file{dir.path() + "/v-take1.values"};
  REQUIRE(file.open(QIODevice::ReadOnly));
  r.file = QJsonDocument::fromJson(file.readAll()).object();
  return r;
}
}

TEST_CASE("Value looper: a take of vectors is a file an automation plays",
          "[fx][values][looper][takes]")
{
  const auto t = written_take([](value_harness& h, int b) {
    if(b == 0)
      h.send(ossia::vec2f{0.f, 10.f}, 0);
    if(b == 12)
      h.send(ossia::vec2f{1.f, 20.f}, 0);
    if(b == 24)
      h.send(ossia::vec2f{0.5f, 15.f}, 0);
  });
  CHECK(t.placeable);
  CHECK(t.file["format"].toString() == "score-value-take");
  CHECK(t.file["frames"].toInt() == 4800);
  CHECK(t.file["components"].toInt() == 2);
  CHECK(t.file["min"].toArray() == QJsonArray{0., 10.});
  CHECK(t.file["max"].toArray() == QJsonArray{1., 20.});
  const auto events = t.file["events"].toArray();
  REQUIRE(events.size() == 3);
  CHECK(events[1].toArray() == QJsonArray{1200, "vec2f", QJsonArray{1., 20.}});
}

TEST_CASE("Value looper: a take of lists of numbers is a file an automation plays",
          "[fx][values][looper][takes]")
{
  using list = std::vector<ossia::value>;
  const auto t = written_take([](value_harness& h, int b) {
    if(b == 0)
      h.send(list{0.f, 1.f, 2.f, 3.f}, 0);
    if(b == 12)
      h.send(list{4.f, 5.f, 6.f, 7.f}, 0);
    if(b == 24)
      h.send(list{1, true}, 0);
  });
  CHECK(t.placeable);
  CHECK(t.file["components"].toInt() == 4);
  CHECK(t.file["min"].toArray() == QJsonArray{0., 1., 2., 3.});
  CHECK(t.file["max"].toArray() == QJsonArray{4., 5., 6., 7.});

  // A list in a list is not numbers.
  const auto nested = written_take([](value_harness& h, int b) {
    if(b == 0)
      h.send(list{list{0.f, 1.f}, 2.f}, 0);
  });
  CHECK(!nested.placeable);
  CHECK(nested.file["components"].toInt() == 0);
}

TEST_CASE("Value looper: the file of a layer alone, placeable as its values are",
          "[fx][values][looper][takes][drag]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  value_harness h;
  h.node.writer = temp_writer(dir);
  h.node.worker.request = [&](Nodes::Looper::value_take_job job) {
    Looper::worker_t::work(std::move(job))(h.node);
  };
  Nodes::Looper::view_message view{};
  h.node.send_message = [&](const Nodes::Looper::view_message& m) { view = m; };

  // Numbers, then an overdub of text.
  four_steps(h);
  h.run_frames(4800);
  h.node.inputs.mode = LoopMode::Overdub;
  h.send(std::string{"a"}, 0);
  h.run_frames(4800);
  h.node.inputs.mode = LoopMode::Play;
  h.run_frames(4800);
  REQUIRE(h.node.loop.count == 2);

  h.node.process_message({.pass = 0, .layers = 2, .mute = false, .take = 1, .last = 0});
  h.run_frames(200);
  REQUIRE(view.part_answer == 1);
  REQUIRE(view.part_files().size() == 1);
  CHECK(view.part_placeable);
  const auto numbers = QString::fromStdString(view.part_files()[0]);
  CHECK(numbers.endsWith("-layer1.values"));
  QFile f{numbers};
  REQUIRE(f.open(QIODevice::ReadOnly));
  const auto json = QJsonDocument::fromJson(f.readAll()).object();
  CHECK(json["components"].toInt() == 1);
  CHECK(json["events"].toArray().size() == 4);

  // The text alone is a file only: the view drags nothing of it.
  h.node.process_message({.pass = 1, .layers = 2, .mute = false, .take = 2, .last = 1});
  h.run_frames(200);
  REQUIRE(view.part_answer == 2);
  CHECK(!view.part_placeable);
  CHECK(QString::fromStdString(view.part_files()[0]).endsWith("-layer2.values"));
}

TEST_CASE("Value looper: a take of text, lists or impulses is a file only",
          "[fx][values][looper][takes]")
{
  const auto t = written_take([](value_harness& h, int b) {
    if(b == 0)
      h.send(std::string{"a \"quoted\" word"}, 0);
    if(b == 12)
      h.send(std::vector<ossia::value>{1, 2.5f}, 0);
    if(b == 20)
      h.send(ossia::impulse{}, 0);
  });
  CHECK(!t.placeable);
  CHECK(t.file["components"].toInt() == 0);
  CHECK(!t.file.contains("min"));
  const auto events = t.file["events"].toArray();
  REQUIRE(events.size() == 3);
  CHECK(events[0].toArray() == QJsonArray{0, "string", "a \"quoted\" word"});
  CHECK(
      events[1].toArray()
      == QJsonArray{1200, "list", QJsonArray{QJsonArray{"int", 1}, QJsonArray{"float", 2.5}}});
  CHECK(events[2].toArray() == QJsonArray{2000, "impulse"});
}

namespace
{
//! The looper of the switch scenarios: a quarter-note grid, ticks of a fifth
//! of a beat, the ticks carrying their place in the bars. Unless `playing` is
//! unset, a new number comes in on each tick.
struct switches_harness : value_harness
{
  static constexpr int64_t beat = 24000;
  static constexpr int64_t frames = 4800;
  std::string_view shown{};
  int shown_count{};
  bool playing{true};
  int counter{};

  switches_harness()
      : value_harness{int(frames)}
  {
    musical = true;
    node.inputs.quantif.value = halp::grid_value::note(0.25);
    node.inputs.postaction_bars.value = 0;
    node.inputs.mode = LoopMode::Stop;
    node.inputs.mode.update_controller = [this](std::string_view s) {
      shown = s;
      shown_count++;
    };
    for(int i = 0; i < 5; i++)
      tick_once();
  }

  void tick_once()
  {
    if(playing)
      send(float(counter++), 100);
    run(frames);
  }
  int passes() const { return node.loop.count; }
  bool muted(int k) const { return node.loop.layers[k].muted; }
  int64_t length() const { return node.loop.length; }
  int64_t position() const
  {
    return node.transport.playbackPos % std::max<int64_t>(1, node.loop.length);
  }
  //! The values of layer k.
  double signature(int k) const
  {
    int n = 0;
    for(int i = 0; i < node.loop.event_count; i++)
      n += node.loop.events[i].pass == k;
    return n;
  }
  void set_max_layers(int n) { node.inputs.max_layers.value = n; }
  void enable_capture() { }
  void stop_score()
  {
    node.stop();
    run(0);
    now = 0;
  }
  //! Merges of value passes are done at once.
  void slow_merges() { }
  //! Takes written in a folder of their own, as the worker would; how many
  //! the view was told of.
  std::unique_ptr<QTemporaryDir> take_dir;
  int take_count{};
  void enable_takes()
  {
    take_dir = std::make_unique<QTemporaryDir>();
    auto w = std::make_shared<Nodes::Looper::take_writer>();
    w->folder = take_dir->path();
    w->prefix = "t";
    w->extension = "take";
    node.writer = w;
    node.worker.request = [this](auto job) { Looper::worker_t::work(std::move(job))(node); };
    node.send_message = [this](const Nodes::Looper::view_message& m) { take_count = m.take; };
  }
  int takes() const { return take_count; }
};
}

#include "LooperSwitchesScenarios.hpp"

TEST_CASE("Value looper switches: an overdub pass is a layer from its first event on",
          "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::lazy_layers(h);
}

TEST_CASE("Value looper switches: recording a loop", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::record_a_loop(h);
}

TEST_CASE("Value looper switches: a recording into an overdub, by After record or by Overdub on an empty loop", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::record_then_overdub(h);
}

TEST_CASE("Value looper switches: play and stop", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::play_and_stop(h);
}

TEST_CASE("Value looper switches: overdub", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::overdub(h);
}

TEST_CASE("Value looper switches: a new layer while overdubbing, without a jump", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::new_layer(h);
}

TEST_CASE("Value looper switches: Retrigger", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::retrigger(h);
}

TEST_CASE("Value looper switches: Start from", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::start_from(h);
}

TEST_CASE("Value looper switches: Undo and Redo", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::undo_redo(h);
}

TEST_CASE("Value looper switches: Mute layer and Erase layer, the newest or a stepped back one", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::mute_and_erase(h);
}

TEST_CASE("Value looper switches: Solo layer", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::solo(h);
}

TEST_CASE("Value looper switches: Toggle reverse, Half speed, Double speed", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::reverse_and_speed(h);
}

TEST_CASE("Value looper switches: At max layers", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::at_max_layers(h);
}

TEST_CASE("Value looper switches: Capture shows Play in Loop", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::capture_shows_play(h);
}

TEST_CASE("Value looper switches: presses before the grid queue", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::presses_queue(h);
}

TEST_CASE("Value looper switches: a cable on Loop asks once", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::cable_on_loop(h);
}

TEST_CASE("Value looper switches: the lanes' m and x act on the grid", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::lanes_on_the_grid(h);
}

TEST_CASE("Value looper switches: Per cycle with Extend", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::per_cycle_extend(h);
}

TEST_CASE("Value looper switches: Layer follows the two oldest layers merging",
          "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::layer_follows_merges(h);
}

TEST_CASE("Value looper switches: a late stop just after a cycle cut leaves no empty layer",
          "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::late_stop_after_cycle_cut(h);
}

TEST_CASE("Value looper switches: undo brings back the layer a pass replaced",
          "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::replace_muted_undo(h);
}

TEST_CASE("Value looper switches: the score stopping closes the layer", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::stop_closes_layer(h);
}

TEST_CASE("Value looper switches: the score started again after a stop plays the loop, Mode untouched",
          "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::restart_after_stop(h);
}

TEST_CASE("Value looper switches: undo while a pass is taken in over a merge into newest keeps the merge",
          "[fx][values][looper][switches][layers]")
{
  switches_harness h;
  looper_switches::undo_while_taking_over_a_merge(h);
}

TEST_CASE("Value looper switches: a solo made while the two oldest layers merge is undone by soloing again",
          "[fx][values][looper][switches][layers]")
{
  switches_harness h;
  looper_switches::solo_while_merging(h);
}

TEST_CASE("Value looper switches: what a recording's bars cut off goes into the overdub after it",
          "[fx][values][looper][switches][layers]")
{
  switches_harness h;
  looper_switches::bars_cut_carried_into_the_overdub(h);
}

TEST_CASE("Value looper switches: a merge loses a muted layer until it is undone, and keeps two muted ones",
          "[fx][values][looper][switches][layers]")
{
  switches_harness h;
  looper_switches::merge_with_muted_layers(h);
}

TEST_CASE("Value looper switches: Record on a loop overdubs it, and records once it is cleared",
          "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::record_overdubs_a_loop(h);
}

TEST_CASE("Value looper switches: Undo or Clear while recording starts it again",
          "[fx][values][looper][switches]")
{
  for(const bool clear : {false, true})
  {
    INFO(clear);
    switches_harness h;
    looper_switches::undo_while_recording(h, clear);
  }
}

TEST_CASE("Value looper switches: three layers of three lengths", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::three_distinct_layers(h);
}

TEST_CASE("Value looper switches: Quantize on the loop", "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::quantif_loop(h);
}

TEST_CASE("Value looper switches: Quantize on the loop before there is one",
          "[fx][values][looper][switches]")
{
  switches_harness h;
  looper_switches::quantif_loop_empty(h);
}

TEST_CASE("Value looper switches: the actions allocate nothing with numbers coming in",
          "[fx][values][looper][switches]")
{
  switches_harness h;
  int layer{};
  bool reverse{};
  float speed{};
  h.node.inputs.layer.update_controller = [&layer](int v) { layer = v; };
  h.node.inputs.reverse.update_controller = [&reverse](bool v) { reverse = v; };
  h.node.inputs.speed.update_controller = [&speed](float v) { speed = v; };
  int allocated = -1;
  {
    alloc_counter::scope counter;
    looper_switches::exercise(h);
    allocated = counter.allocations();
  }
  CHECK(allocated == 0);
  CHECK(h.passes() == 2);
}

TEST_CASE("Value looper switches: Capture on an empty loop, undone as a first recording",
          "[fx][values][looper][switches][capture]")
{
  switches_harness h;
  looper_switches::capture_on_empty(h);
}

TEST_CASE("Value looper switches: Capture over a loop adds its last round as a layer",
          "[fx][values][looper][switches][capture]")
{
  switches_harness h;
  looper_switches::capture_adds_layer(h);
}

TEST_CASE("Value looper switches: Capture over a stopped loop",
          "[fx][values][looper][switches][capture]")
{
  switches_harness h;
  looper_switches::capture_over_stopped_loop(h);
}

TEST_CASE("Value looper switches: a captured layer at Max layers",
          "[fx][values][looper][switches][capture]")
{
  switches_harness h;
  looper_switches::capture_at_max_layers(h);
}

TEST_CASE("Value looper switches: Capture keeps the mutes", "[fx][values][looper][switches][capture]")
{
  switches_harness h;
  looper_switches::capture_keeps_mutes(h);
}

TEST_CASE("Value looper switches: Capture while taking in does nothing",
          "[fx][values][looper][switches][capture]")
{
  switches_harness h;
  looper_switches::capture_while_taking(h);
}

TEST_CASE("Value looper switches: a round with nothing new captures no layer",
          "[fx][values][looper][switches][capture]")
{
  switches_harness h;
  looper_switches::capture_lazy_layers(h);
}

TEST_CASE("Value looper switches: Capture allocates nothing", "[fx][values][looper][switches][capture]")
{
  switches_harness h;
  h.enable_capture();
  int allocated = -1;
  {
    alloc_counter::scope counter;
    looper_switches::capture_exercise(h);
    allocated = counter.allocations();
  }
  CHECK(allocated == 0);
  CHECK(h.passes() >= 2);
}

TEST_CASE("Value looper: Capture over a loop lays its last round where it came",
          "[fx][values][looper][capture]")
{
  using H = switches_harness;
  for(const bool late : {false, true})
  {
    CAPTURE(late);
    H h;
    auto& in = h.node.inputs;
    looper_switches::record_loop(h);
    in.late.value = late ? 150 : 0;
    looper_switches::beats(h, 3);
    while(h.now % H::beat != H::frames)
      h.tick_once();
    const int64_t now = h.now, p0 = h.position(), len = h.length();
    const int next = h.counter;
    looper_switches::press(h, in.capture);
    REQUIRE(h.passes() == 2);
    REQUIRE(h.length() == len);

    const int64_t end = late ? now - H::frames : now;
    std::vector<std::pair<int64_t, float>> expected;
    for(int j = 1; j <= 30; j++)
    {
      const int64_t t = now - j * H::frames + 100;
      if(t < end - len || t >= end)
        continue;
      expected.emplace_back(((p0 - (now - t)) % len + len) % len, float(next - j));
    }
    std::sort(expected.begin(), expected.end());
    const auto e = h.events(1);
    REQUIRE(e.size() == expected.size());
    CHECK(expected.size() == 20);
    for(std::size_t i = 0; i < e.size(); i++)
    {
      CHECK(e[i].first == expected[i].first);
      CHECK(same(e[i].second, expected[i].second));
    }
  }
}
