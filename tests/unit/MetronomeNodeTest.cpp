// The metronome's engine, driven directly: every click on the sample its beat
// falls on, whatever the buffers; the accent on the bar line of 4/4, 7/8 and
// 19/16; a change of tempo, of signature, a jump of the transport; and what
// the controls do to the sound.

#include "AllocCounter.hpp"

#include <Media/Metro/MetroNode.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace
{
using namespace Media::Metro;

constexpr int rate = 48000;
constexpr int64_t flicks_per_sample = ossia::flicks_per_second<int64_t> / rate;

//! One sample of a known level, so that the output tells which sound played
//! and on which sample.
click_sound impulse(float level, int frames = 4)
{
  click_sound s;
  s.left.assign(frames, 0.f);
  s.left[0] = level;
  return s;
}

constexpr float accent_level = 1.f;
constexpr float beat_level = 0.5f;
constexpr float sub_level = beat_level * metronome_node::subdivision_level;

/**
 * A transport: plays buffers of any size, and keeps the musical position and
 * the bar lines as the engine reports them. The positions are computed from
 * whole numbers of samples since the last change of tempo, so that a beat is
 * exactly on a sample when the test says so.
 */
struct rig
{
  metronome_node node{rate};
  ossia::execution_state st;

  double tempo{120.};
  ossia::time_signature sig{4, 4};

  //! Samples played, and the same along the time line: a jump moves the
  //! second one only.
  int64_t played{};
  int64_t date{};

  //! Musical position and samples played at the last change of tempo or jump.
  double anchor_position{};
  int64_t anchor_played{};
  double last_bar{};

  //! What ossia::token_request::metronome() gives for the same ticks: the
  //! sample and whether it is a bar line. The Metronome played exactly this
  //! grid when a beat was always a quarter note.
  std::vector<std::pair<int64_t, bool>> reference;

  //! Samples, counted from the first one played.
  std::vector<int64_t> pulses;
  std::vector<double> left;
  std::vector<double> right;

  rig()
  {
    st.sampleRate = rate;
    st.modelToSamplesRatio = 1. / flicks_per_sample;

    auto set = std::make_shared<sound_set>();
    set->accent = impulse(accent_level);
    set->beat = impulse(beat_level);
    sound_set_ptr p = set;
    node.swap_sounds(Files, p);
    node.sound = Files;
  }

  double position(int64_t at) const noexcept
  {
    return anchor_position + double(at - anchor_played) * tempo / (60. * rate);
  }
  double bar_length() const noexcept { return 4. * sig.upper / sig.lower; }

  void set_tempo(double t)
  {
    anchor_position = position(played);
    anchor_played = played;
    tempo = t;
  }

  //! The transport moves to a musical position, in a bar that starts at `bar`.
  void jump(double musical, double bar)
  {
    anchor_position = musical;
    anchor_played = played;
    last_bar = bar;
    date = std::llround(musical * 60. / tempo * rate);
  }

  //! The signature changes on the bar line the transport is on.
  void set_signature(ossia::time_signature s)
  {
    sig = s;
    last_bar = position(played);
  }

  void tick(int frames, bool rewind = false)
  {
    st.bufferSize = frames;
    const double start = position(played);
    const double end = rewind ? start - (position(played + frames) - start)
                              : position(played + frames);
    const int64_t next_date = rewind ? date - frames : date + frames;

    ossia::token_request tk{
        ossia::time_value{date * flicks_per_sample},
        ossia::time_value{next_date * flicks_per_sample},
        ossia::time_value{ossia::flicks_per_second<int64_t> * 100000},
        ossia::time_value{0},
        rewind ? -1. : 1.,
        sig,
        tempo};
    tk.start_sample = 0;
    tk.length_sample = frames;
    tk.musical_start_position = start;
    tk.musical_start_last_bar = last_bar;
    double end_bar = last_bar;
    while(!rewind && end >= end_bar + bar_length())
      end_bar += bar_length();
    while(rewind && end < end_bar)
      end_bar -= bar_length();
    tk.musical_end_position = end;
    tk.musical_end_last_bar = end_bar;

    tk.metronome(
        st.modelToSamplesRatio,
        [&](int64_t at) { reference.emplace_back(played + at, true); },
        [&](int64_t at) { reference.emplace_back(played + at, false); });

    auto& audio = *node.audio_out.target<ossia::audio_port>();
    auto& bangs = *node.bang_out.target<ossia::value_port>();
    // As the graph leaves the outlets before a node runs.
    audio.set_channels(0);
    bangs.clear();

    ossia::exec_state_facade fac{&st};
    static_cast<ossia::graph_node&>(node).run(tk, fac);

    for(auto& v : bangs.get_data())
      pulses.push_back(played + v.timestamp);
    const auto& out = audio.get();
    REQUIRE(out.size() == 2);
    REQUIRE(out[0].size() >= std::size_t(frames));
    left.insert(left.end(), out[0].begin(), out[0].begin() + frames);
    right.insert(right.end(), out[1].begin(), out[1].begin() + frames);

    if(rewind)
    {
      // The rig only rewinds for one tick at a time.
      anchor_position = end;
      anchor_played = played + frames;
    }
    played += frames;
    date = next_date;
    last_bar = end_bar;
  }

  void play(int64_t frames, int buffer)
  {
    while(frames > 0)
    {
      const int n = int(std::min<int64_t>(frames, buffer));
      tick(n);
      frames -= n;
    }
  }

  //! The level of the sample each pulse is on.
  std::vector<double> levels() const
  {
    std::vector<double> res;
    for(auto p : pulses)
      res.push_back(left[p]);
    return res;
  }

  double energy() const
  {
    double e = 0.;
    for(double v : left)
      e += v * v;
    return e;
  }
};

std::vector<int64_t> every(int64_t period, int count, int64_t from = 0)
{
  std::vector<int64_t> res;
  for(int i = 0; i < count; i++)
    res.push_back(from + i * period);
  return res;
}

//! `accent` then `beats - 1` plain beats, `bars` times.
std::vector<double> bars_of(int beats, int bars)
{
  std::vector<double> res;
  for(int b = 0; b < bars; b++)
    for(int i = 0; i < beats; i++)
      res.push_back(i == 0 ? accent_level : beat_level);
  return res;
}
}

TEST_CASE("metronome: 4/4, a click on the sample of every quarter note", "[metronome]")
{
  // 120 bpm: a quarter note is 24000 samples, which no buffer size here
  // divides but 480: the clicks fall inside the buffers.
  for(int buffer : {64, 333, 480, 1024, 4096})
  {
    INFO("buffer of " << buffer);
    rig r;
    r.play(8 * 24000, buffer);
    CHECK(r.pulses == every(24000, 8));
    CHECK(r.levels() == bars_of(4, 2));
    // Nothing but the clicks: the sound is one sample long.
    double others = 0.;
    for(std::size_t i = 0; i < r.left.size(); i++)
      if(i % 24000 != 0)
        others += std::abs(r.left[i]);
    CHECK(others == 0.);
    CHECK(r.left == r.right);
  }
}

TEST_CASE("metronome: 7/8, a click on every eighth note", "[metronome]")
{
  for(int buffer : {64, 333, 1024})
  {
    INFO("buffer of " << buffer);
    rig r;
    r.sig = {7, 8};
    r.play(21 * 12000, buffer);
    CHECK(r.pulses == every(12000, 21));
    CHECK(r.levels() == bars_of(7, 3));
  }
}

TEST_CASE("metronome: 19/16, a click on every sixteenth note", "[metronome]")
{
  for(int buffer : {64, 333, 1024})
  {
    INFO("buffer of " << buffer);
    rig r;
    r.sig = {19, 16};
    r.play(38 * 6000, buffer);
    CHECK(r.pulses == every(6000, 38));
    CHECK(r.levels() == bars_of(19, 2));
  }
}

TEST_CASE("metronome: a change of tempo moves the next clicks, not the grid", "[metronome]")
{
  for(int buffer : {125, 480, 1000})
  {
    INFO("buffer of " << buffer);
    rig r;
    // A beat and a half at 120, then 90: the half beat left lasts 16000
    // samples, and a quarter note 32000.
    r.play(36000, buffer);
    r.set_tempo(90.);
    r.play(3 * 32000, buffer);
    CHECK(r.pulses == std::vector<int64_t>{0, 24000, 52000, 84000, 116000});
    CHECK(
        r.levels()
        == std::vector<double>{
            accent_level, beat_level, beat_level, beat_level, accent_level});
  }
}

TEST_CASE("metronome: a tempo that changes on every buffer", "[metronome]")
{
  // An accelerando: no click is lost or played twice, and the accents stay
  // every four beats.
  rig r;
  for(int i = 0; i < 2000; i++)
  {
    r.set_tempo(60. + i * 0.1);
    r.tick(256);
  }
  const double beats = r.position(r.played);
  REQUIRE(r.pulses.size() == std::size_t(std::ceil(beats)));
  const auto levels = r.levels();
  for(std::size_t i = 0; i < levels.size(); i++)
    CHECK(levels[i] == (i % 4 == 0 ? accent_level : beat_level));
  for(std::size_t i = 1; i < r.pulses.size(); i++)
    CHECK(r.pulses[i] > r.pulses[i - 1]);
}

TEST_CASE("metronome: a jump of the transport", "[metronome]")
{
  for(int buffer : {64, 333, 1024})
  {
    INFO("buffer of " << buffer);
    rig r;
    r.play(30000, buffer);
    // To the middle of the third beat of the second bar.
    r.jump(6.5, 4.);
    r.play(48000, buffer);
    // Onto a bar line: its click is the first sample played.
    r.jump(12., 12.);
    r.play(24000, buffer);
    // Back to the start.
    r.jump(0., 0.);
    r.play(1000, buffer);

    CHECK(
        r.pulses
        == std::vector<int64_t>{0, 24000, 30000 + 12000, 30000 + 36000, 78000, 102000});
    CHECK(
        r.levels()
        == std::vector<double>{
            accent_level, beat_level, beat_level, accent_level, accent_level,
            accent_level});
  }
}

TEST_CASE("metronome: a change of signature starts a bar", "[metronome]")
{
  rig r;
  r.play(8 * 24000, 333);
  r.set_signature({3, 4});
  r.play(6 * 24000, 333);
  r.set_signature({5, 8});
  r.play(10 * 12000, 333);

  auto expected = every(24000, 14);
  for(auto p : every(12000, 10, 14 * 24000))
    expected.push_back(p);
  CHECK(r.pulses == expected);

  auto levels = bars_of(4, 2);
  for(auto l : bars_of(3, 2))
    levels.push_back(l);
  for(auto l : bars_of(5, 2))
    levels.push_back(l);
  CHECK(r.levels() == levels);
}

TEST_CASE("metronome: rewinding plays the clicks it goes back over", "[metronome]")
{
  rig r;
  r.jump(4.5, 4.);
  // Half a beat back: the bar line is the last sample but one of the tick,
  // counted from its end.
  r.tick(12000 + 480, true);
  REQUIRE(r.pulses.size() == 1);
  CHECK(r.pulses[0] == 12000);
  CHECK(r.levels() == std::vector<double>{accent_level});
}

TEST_CASE("metronome: subdivisions are quieter clicks between the beats", "[metronome]")
{
  rig r;
  r.node.subdivision = 2;
  r.play(4 * 24000, 333);
  CHECK(r.pulses == every(12000, 8));
  CHECK(
      r.levels()
      == std::vector<double>{
          accent_level, sub_level, beat_level, sub_level, beat_level, sub_level,
          beat_level, sub_level});

  rig t;
  t.node.subdivision = 3;
  t.play(2 * 24000, 333);
  CHECK(t.pulses == every(8000, 6));
}

TEST_CASE("metronome: without accent the bar line is a beat as the others", "[metronome]")
{
  rig r;
  r.node.accent = false;
  r.play(5 * 24000, 333);
  CHECK(r.levels() == std::vector<double>(5, double(beat_level)));
}

TEST_CASE("metronome: one file is enough for both", "[metronome]")
{
  rig r;
  auto set = std::make_shared<sound_set>();
  set->beat = impulse(0.25f);
  sound_set_ptr p = set;
  r.node.swap_sounds(Files, p);
  r.play(2 * 24000, 333);
  CHECK(r.levels() == std::vector<double>{0.25, 0.25});
}

TEST_CASE("metronome: the volume scales the clicks, and moves without a step", "[metronome]")
{
  rig r;
  r.node.volume = 0.5f;
  r.play(2 * 24000, 333);
  CHECK(r.levels() == std::vector<double>{0.5 * accent_level, 0.5 * beat_level});

  // Silent, the pulses still go out.
  rig s;
  s.node.volume = 0.f;
  s.play(2 * 24000, 333);
  CHECK(s.pulses.size() == 2);
  CHECK(s.energy() == 0.);

  // A long sound of a constant level shows the volume itself.
  rig m;
  auto set = std::make_shared<sound_set>();
  set->accent.left.assign(rate, 1.f);
  set->beat = set->accent;
  sound_set_ptr p = set;
  m.node.swap_sounds(Files, p);
  m.tick(1000);
  m.node.volume = 0.f;
  m.tick(1000);
  m.tick(1000);
  CHECK(m.left[999] == 1.);
  CHECK(m.left[1500] == Catch::Approx(0.5).margin(0.01));
  CHECK(m.left[2000] == 0.);
  for(std::size_t i = 1; i < m.left.size(); i++)
    REQUIRE(std::abs(m.left[i] - m.left[i - 1]) < 0.0011);
}

TEST_CASE("metronome: a click still sounding fades out under the next", "[metronome]")
{
  // A sound longer than a beat, of a constant level.
  rig r;
  auto set = std::make_shared<sound_set>();
  set->accent.left.assign(2 * rate, 1.f);
  set->beat = set->accent;
  sound_set_ptr p = set;
  r.node.swap_sounds(Files, p);
  r.play(3 * 24000, 333);

  // 5 ms after the second click, only the second one is left.
  const int fade = rate / 200;
  CHECK(r.left[23999] == 1.);
  CHECK(r.left[24000] > 1.9);
  CHECK(r.left[24000 + fade / 2] == Catch::Approx(1.5).margin(0.01));
  CHECK(r.left[24000 + fade] == 1.);
  CHECK(r.left[47999] == 1.);
  // The fade is a ramp: no step but the start of the next click.
  for(int i = 24001; i < 48000; i++)
    REQUIRE(std::abs(r.left[i] - r.left[i - 1]) < 1.5 / fade);
}

TEST_CASE("metronome: the Sound control changes what is heard", "[metronome]")
{
  std::vector<std::vector<double>> rendered;
  for(int kind : {Click, Woodblock, Beep, Cowbell})
  {
    rig r;
    r.node.sound = kind;
    r.play(8 * 24000, 333);
    CHECK(r.pulses == every(24000, 8));
    CHECK(r.energy() > 1.);
    CHECK(r.left == r.right);
    for(double v : r.left)
      REQUIRE(std::abs(v) <= 1.);
    // The accent is not the beat.
    CHECK(
        std::vector<double>(r.left.begin(), r.left.begin() + 1000)
        != std::vector<double>(r.left.begin() + 24000, r.left.begin() + 25000));
    // Two beats are the same click.
    CHECK(
        std::vector<double>(r.left.begin() + 24000, r.left.begin() + 25000)
        == std::vector<double>(r.left.begin() + 48000, r.left.begin() + 49000));
    rendered.push_back(r.left);
  }
  for(std::size_t i = 0; i < rendered.size(); i++)
    for(std::size_t j = i + 1; j < rendered.size(); j++)
      CHECK(rendered[i] != rendered[j]);

  // The control is followed while playing, from the next click on.
  rig r;
  r.node.sound = Beep;
  r.play(24000, 333);
  r.node.sound = Woodblock;
  r.play(24000, 333);
  rig beep;
  beep.node.sound = Beep;
  beep.play(48000, 333);
  CHECK(
      std::vector<double>(r.left.begin(), r.left.begin() + 24000)
      == std::vector<double>(beep.left.begin(), beep.left.begin() + 24000));
  CHECK(
      std::vector<double>(r.left.begin() + 24000, r.left.end())
      != std::vector<double>(beep.left.begin() + 24000, beep.left.end()));
}

TEST_CASE("metronome: sounds that are not there yet play as the click", "[metronome]")
{
  // The classic ticks of a library that is not installed, files still
  // decoding, a value out of the choices.
  rig click;
  click.node.sound = Click;
  click.play(4 * 24000, 333);

  for(int kind : {int(Classic), 12, -3})
  {
    rig r;
    r.node.sound = kind;
    r.play(4 * 24000, 333);
    CHECK(r.left == click.left);
  }

  rig r;
  sound_set_ptr none;
  r.node.swap_sounds(Files, none);
  r.play(4 * 24000, 333);
  CHECK(r.left == click.left);
}

TEST_CASE("metronome: a stereo file keeps its channels", "[metronome]")
{
  rig r;
  auto set = std::make_shared<sound_set>();
  set->accent = impulse(1.f);
  set->accent.right.assign(4, 0.f);
  set->accent.right[1] = -1.f;
  set->beat = set->accent;
  sound_set_ptr p = set;
  r.node.swap_sounds(Files, p);
  r.play(1000, 333);
  CHECK(r.left[0] == 1.);
  CHECK(r.right[0] == 0.);
  CHECK(r.right[1] == -1.);
}

TEST_CASE("metronome: new sounds stop the clicks playing the old ones", "[metronome]")
{
  rig r;
  auto set = std::make_shared<sound_set>();
  set->accent.left.assign(rate, 1.f);
  set->beat = set->accent;
  sound_set_ptr p = set;
  r.node.swap_sounds(Files, p);
  r.tick(1000);
  CHECK(r.left[999] == 1.);

  // What comes back is what the node had, alone: nothing reads it any more.
  sound_set_ptr next = std::make_shared<sound_set>();
  r.node.swap_sounds(Files, next);
  CHECK(next == set);
  set.reset();
  CHECK(next.use_count() == 1);
  next.reset();
  r.tick(1000);
  CHECK(r.left[1000] == 0.);
  CHECK(r.left[1999] == 0.);
}

TEST_CASE("metronome: the built-in sounds follow the rate and end on a zero", "[metronome]")
{
  for(int k : {Click, Woodblock, Beep, Cowbell})
  {
    for(bool accent : {true, false})
    {
      const auto a = make_builtin_sound(sound_kind(k), accent, 48000);
      const auto b = make_builtin_sound(sound_kind(k), accent, 96000);
      const auto c = make_builtin_sound(sound_kind(k), accent, 22050);
      REQUIRE(!a.empty());
      // The same duration at every rate
      CHECK(b.frames() == 2 * a.frames());
      CHECK(double(c.frames()) / 22050. == Catch::Approx(a.frames() / 48000.).epsilon(0.01));
      CHECK(a.right.empty());
      CHECK(a.left.back() == 0.f);
      CHECK(b.left.back() == 0.f);
      CHECK(c.left.back() == 0.f);
      CHECK(a.left.front() == 0.f);
    }
  }
}

TEST_CASE("metronome: playing does not allocate", "[metronome]")
{
  rig r;
  r.node.sound = Cowbell;
  r.node.subdivision = 4;
  r.pulses.reserve(100000);
  r.left.reserve(10 * rate);
  r.right.reserve(10 * rate);
  // The buffers of the port and of the pulses come out of pools the first
  // ticks fill.
  r.play(rate, 512);

  auto& audio = *r.node.audio_out.target<ossia::audio_port>();
  auto& bangs = *r.node.bang_out.target<ossia::value_port>();
  r.st.bufferSize = 512;
  ossia::exec_state_facade fac{&r.st};
  int allocations = 0;
  for(int i = 0; i < 200; i++)
  {
    ossia::token_request tk{
        ossia::time_value{r.date * flicks_per_sample},
        ossia::time_value{(r.date + 512) * flicks_per_sample},
        ossia::time_value{ossia::flicks_per_second<int64_t> * 100000},
        ossia::time_value{0},
        1.,
        r.sig,
        r.tempo};
    tk.start_sample = 0;
    tk.length_sample = 512;
    tk.musical_start_position = r.position(r.played);
    tk.musical_end_position = r.position(r.played + 512);
    tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
    tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
    r.node.sound = Click + (i % 4);
    r.node.volume = 0.5f + 0.001f * i;
    {
      alloc_counter::scope scope;
      static_cast<ossia::graph_node&>(r.node).run(tk, fac);
      allocations += scope.allocations();
    }
    audio.set_channels(0);
    bangs.clear();
    r.played += 512;
    r.date += 512;
  }
  CHECK(allocations == 0);
}

TEST_CASE("metronome: a file inlet hands over new paths only", "[metronome]")
{
  rig r;
  std::vector<std::pair<int, std::string>> asked;
  r.node.on_file = [&](int file, const std::string& path) { asked.emplace_back(file, path); };
  r.node.set_file_path(0, "model.wav");
  auto& accent = *r.node.accent_file_in;
  auto& beat = *r.node.beat_file_in;
  const auto run = [&] {
    r.tick(256);
    accent.clear();
    beat.clear();
  };

  // What the inlet is already on
  accent.write_value(std::string{"model.wav"}, 0);
  run();
  CHECK(asked.empty());

  accent.write_value(std::string{"a.wav"}, 0);
  run();
  REQUIRE(asked.size() == 1);
  CHECK(asked[0] == std::pair<int, std::string>{0, "a.wav"});

  // Repeated on every tick, and nothing at all
  for(int i = 0; i < 10; i++)
  {
    accent.write_value(std::string{"a.wav"}, 0);
    run();
  }
  run();
  CHECK(asked.size() == 1);

  // Several in a tick: the last one
  accent.write_value(std::string{"b.wav"}, 0);
  accent.write_value(std::string{"c.wav"}, 10);
  accent.write_value(std::string{"d.wav"}, 20);
  run();
  REQUIRE(asked.size() == 2);
  CHECK(asked[1] == std::pair<int, std::string>{0, "d.wav"});

  // The two inlets are apart
  beat.write_value(std::string{"d.wav"}, 0);
  run();
  REQUIRE(asked.size() == 3);
  CHECK(asked[2] == std::pair<int, std::string>{1, "d.wav"});

  // No path is a path: the inlet has no file any more
  accent.write_value(std::string{}, 0);
  run();
  REQUIRE(asked.size() == 4);
  CHECK(asked[3] == std::pair<int, std::string>{0, ""});

  // Not a string
  accent.write_value(1, 0);
  beat.write_value(ossia::impulse{}, 0);
  run();
  CHECK(asked.size() == 4);
}

TEST_CASE("metronome: taking a path does not allocate", "[metronome]")
{
  rig r;
  int asked = 0;
  r.node.on_file = [&](int, const std::string&) { asked++; };
  r.play(4096, 512);
  r.st.bufferSize = 512;

  auto& accent = *r.node.accent_file_in;
  ossia::exec_state_facade fac{&r.st};
  // As long as paths get, and written in the port before the tick: the
  // values of a port are its source's.
  std::vector<ossia::value> paths;
  for(int i = 0; i < 16; i++)
    paths.emplace_back(
        "/home/someone/Documents/a rather long folder name/samples/metronomes/click number "
        + std::to_string(i) + ".wav");

  int allocations = 0;
  for(int i = 0; i < 16; i++)
  {
    accent.write_value(paths[i], 0);
    ossia::token_request tk{
        ossia::time_value{r.date * flicks_per_sample},
        ossia::time_value{(r.date + 512) * flicks_per_sample},
        ossia::time_value{ossia::flicks_per_second<int64_t> * 100000},
        ossia::time_value{0},
        1.,
        r.sig,
        r.tempo};
    tk.start_sample = 0;
    tk.length_sample = 512;
    tk.musical_start_position = r.position(r.played);
    tk.musical_end_position = r.position(r.played + 512);
    tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
    tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
    {
      alloc_counter::scope scope;
      static_cast<ossia::graph_node&>(r.node).run(tk, fac);
      allocations += scope.allocations();
    }
    accent.clear();
    r.node.audio_out.target<ossia::audio_port>()->set_channels(0);
    r.node.bang_out.target<ossia::value_port>()->clear();
    r.played += 512;
    r.date += 512;
  }
  CHECK(asked == 16);
  CHECK(allocations == 0);
}

namespace
{
//! A stretch of bars in one signature.
struct stretch
{
  ossia::time_signature sig;
  int bars;
};

//! Where the clicks are and what they are, written out from the definition of
//! the two grids, at 120 bpm: a quarter note is 24000 samples.
std::vector<std::pair<int64_t, double>>
expected_clicks(int unit, int subdivision, std::vector<stretch> stretches)
{
  std::vector<std::pair<int64_t, double>> res;
  int64_t bar_start = 0;
  for(auto [sig, bars] : stretches)
  {
    // Whole numbers of samples for every signature and subdivision used here.
    const int64_t bar = 96000 * sig.upper / sig.lower;
    const int64_t beat = unit == SignatureBeat ? 96000 / sig.lower : 24000;
    const int64_t step = beat / subdivision;
    for(int b = 0; b < bars; b++, bar_start += bar)
      for(int64_t k = 0; k * step < bar; k++)
        res.emplace_back(
            bar_start + k * step,
            k == 0 ? accent_level : (k % subdivision == 0 ? beat_level : sub_level));
  }
  return res;
}

int64_t samples_of(const std::vector<stretch>& stretches)
{
  int64_t n = 0;
  for(auto [sig, bars] : stretches)
    n += 96000 * sig.upper / sig.lower * bars;
  return n;
}

//! Plays the stretches and returns each click with its level.
std::vector<std::pair<int64_t, double>>
played_clicks(rig& r, const std::vector<stretch>& stretches, int buffer)
{
  for(auto [sig, bars] : stretches)
  {
    r.set_signature(sig);
    r.play(96000 * sig.upper / sig.lower * bars, buffer);
  }
  std::vector<std::pair<int64_t, double>> res;
  const auto levels = r.levels();
  for(std::size_t i = 0; i < r.pulses.size(); i++)
    res.emplace_back(r.pulses[i], levels[i]);
  return res;
}
}

TEST_CASE("metronome: both beats, in 4/4, 6/8, 7/8 and 19/16", "[metronome]")
{
  for(ossia::time_signature sig :
      {ossia::time_signature{4, 4}, ossia::time_signature{6, 8},
       ossia::time_signature{7, 8}, ossia::time_signature{19, 16}})
    for(int unit : {int(SignatureBeat), int(QuarterBeat)})
      for(int subdivision : {1, 2})
        for(int buffer : {64, 333, 1024})
        {
          INFO(
              sig.upper << "/" << sig.lower << " unit " << unit << " subdivision "
                        << subdivision << " buffer " << buffer);
          rig r;
          r.node.beat = unit;
          r.node.subdivision = subdivision;
          const std::vector<stretch> bars{{sig, 3}};
          CHECK(played_clicks(r, bars, buffer) == expected_clicks(unit, subdivision, bars));
        }
}

TEST_CASE("metronome: quarter notes in bars they do not divide", "[metronome]")
{
  // 7/8 is three quarter notes and a half: the accent, three beats, and the
  // next accent an eighth note after the place of a fourth beat.
  {
    rig r;
    r.node.beat = QuarterBeat;
    r.sig = {7, 8};
    r.play(2 * 84000, 333);
    CHECK(
        r.pulses
        == std::vector<int64_t>{0, 24000, 48000, 72000, 84000, 108000, 132000, 156000});
    CHECK(r.levels() == bars_of(4, 2));
  }
  // 19/16 is four quarter notes and three sixteenths.
  {
    rig r;
    r.node.beat = QuarterBeat;
    r.sig = {19, 16};
    r.play(2 * 114000, 333);
    CHECK(
        r.pulses
        == std::vector<int64_t>{0, 24000, 48000, 72000, 96000, 114000, 138000, 162000,
                                186000, 210000});
    CHECK(r.levels() == bars_of(5, 2));
  }
  // Subdivided in two, the eighth notes of 7/8 all have a click, and what
  // tells the two beats apart is which of them are the quiet ones.
  {
    rig r;
    r.node.beat = QuarterBeat;
    r.node.subdivision = 2;
    r.sig = {7, 8};
    r.play(84000, 333);
    CHECK(r.pulses == every(12000, 7));
    CHECK(
        r.levels()
        == std::vector<double>{
            accent_level, sub_level, beat_level, sub_level, beat_level, sub_level,
            beat_level});

    rig s;
    s.sig = {7, 8};
    s.play(84000, 333);
    CHECK(s.pulses == every(12000, 7));
    CHECK(s.levels() == bars_of(7, 1));
  }
}

TEST_CASE("metronome: both beats across changes of signature", "[metronome]")
{
  const std::vector<stretch> bars{
      {{4, 4}, 2}, {{7, 8}, 2}, {{6, 8}, 1}, {{19, 16}, 2}, {{3, 4}, 2}, {{7, 8}, 1}};
  for(int unit : {int(SignatureBeat), int(QuarterBeat)})
    for(int subdivision : {1, 2})
      for(int buffer : {64, 333, 1024})
      {
        INFO("unit " << unit << " subdivision " << subdivision << " buffer " << buffer);
        rig r;
        r.node.beat = unit;
        r.node.subdivision = subdivision;
        CHECK(played_clicks(r, bars, buffer) == expected_clicks(unit, subdivision, bars));
      }
}

TEST_CASE("metronome: quarter notes are the grid the metronome always had", "[metronome]")
{
  // Against ossia::token_request::metronome(), which placed the clicks before
  // the Beat control: the same samples and the same accents, for the same
  // ticks, whatever the signature, the tempo and the jumps.
  const std::vector<stretch> bars{
      {{4, 4}, 2}, {{7, 8}, 2}, {{6, 8}, 2}, {{19, 16}, 2}, {{5, 4}, 1}, {{3, 8}, 3}};
  for(int buffer : {64, 333, 1024})
  {
    INFO("buffer of " << buffer);
    rig r;
    r.node.beat = QuarterBeat;
    played_clicks(r, bars, buffer);
    r.set_signature({4, 4});
    r.set_tempo(93.);
    r.play(100000, buffer);
    r.jump(6.5, 4.);
    r.play(70000, buffer);
    r.sig = {7, 8};
    r.jump(7., 7.);
    for(int i = 0; i < 300; i++)
    {
      r.set_tempo(70. + i * 0.37);
      r.tick(buffer);
    }

    REQUIRE(r.pulses.size() > 40);
    REQUIRE(r.pulses.size() == r.reference.size());
    const auto levels = r.levels();
    for(std::size_t i = 0; i < r.pulses.size(); i++)
    {
      REQUIRE(r.pulses[i] == r.reference[i].first);
      REQUIRE(levels[i] == (r.reference[i].second ? accent_level : beat_level));
    }
  }

  // The other beat is another grid as soon as the unit is not a quarter note
  rig r;
  r.sig = {7, 8};
  r.play(84000, 333);
  CHECK(r.pulses.size() != r.reference.size());
}

TEST_CASE("metronome: the Beat inlet is followed while playing", "[metronome]")
{
  rig r;
  r.sig = {6, 8};
  r.play(72000, 333);
  r.node.beat_in->write_value(int(QuarterBeat), 0);
  r.tick(256);
  r.node.beat_in->clear();
  r.play(72000 - 256, 333);
  // A bar of six eighth notes, then the same bar as three quarter notes
  CHECK(
      r.pulses
      == std::vector<int64_t>{0, 12000, 24000, 36000, 48000, 60000, 72000, 96000, 120000});
}
