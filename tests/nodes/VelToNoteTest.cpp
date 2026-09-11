// SPDX-License-Identifier: GPL-3.0-or-later
#include <Fx/VelToNote.hpp>

#include <ossia/dataflow/execution_state.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <memory>
#include <vector>

namespace
{
using namespace Nodes::PulseToNote::detail;
using Nodes::PulseToNote::Node;

Block block(
    int first, int frames, double q0, double q1, std::int64_t m0, std::int64_t m1)
{
  return {
      .frames = frames,
      .first_frame = first,
      .quarters_begin = q0,
      .quarters_end = q1,
      .bar_begin = std::floor(q0 / 4.) * 4.,
      .bar_end = std::floor(q1 / 4.) * 4.,
      .model_begin = m0,
      .model_end = m1};
}

struct Processor
{
  ossia::execution_state state;
  ossia::value_port input;
  Node node;

  Processor()
  {
    state.sampleRate = 1000;
    state.modelToSamplesRatio = 1000. / flicks_per_second;
    state.samplesToModelRatio = flicks_per_second / 1000.;
    node.ossia_state = {&state};
    node.inputs.port.value = &input;
    node.inputs.start_quant.value = 0.;
    node.inputs.end_quant.value = 0.;
    node.inputs.basenote.value = 60;
    node.inputs.basevel.value = 100;
    node.prepare({64, 1000., 17});
  }

  ossia::token_request token(const Block& b, int offset = 0, int callback_size = 0)
  {
    if(!callback_size)
      callback_size = offset + b.frames;
    state.bufferSize = callback_size;
    state.samples_since_start = b.first_frame - offset + callback_size;
    ossia::token_request tk;
    tk.prev_date.impl = b.model_begin;
    tk.date.impl = b.model_end;
    tk.start_sample = offset;
    tk.length_sample = b.frames;
    tk.musical_start_position = b.quarters_begin;
    tk.musical_end_position = b.quarters_end;
    tk.musical_start_last_bar = b.bar_begin;
    tk.musical_end_last_bar = b.bar_end;
    tk.signature
        = {static_cast<std::uint16_t>(b.numerator),
           static_cast<std::uint16_t>(b.denominator)};
    tk.start_discontinuous = b.discontinuity;
    tk.end_discontinuous = b.end_discontinuity;
    return tk;
  }

  std::vector<MidiEvent> output() const
  {
    std::vector<MidiEvent> result;
    for(const auto& m : node.outputs.midi.midi_messages)
      result.push_back(
          {static_cast<int>(m.timestamp), static_cast<std::uint8_t>(m.bytes[0] & 15),
           m.bytes[1], m.bytes[2], (m.bytes[0] & 0xf0) == 0x90});
    return result;
  }
};
}

TEST_CASE(
    "VelToNote owns deferred offs independently of real output storage",
    "[nodes][veltonote]")
{
  auto p = std::make_unique<Processor>();
  p->input.write_value(60, 7);
  auto first = p->token(block(4, 4, 0., .008, 0, 2822400), 4, 16);
  p->node(first);
  REQUIRE(p->output() == std::vector<MidiEvent>{{3, 0, 60, 100, true}});
  {
    auto published = std::move(p->node.outputs.midi.midi_messages);
    REQUIRE(published.front().timestamp == 3);
  } // The former port's allocation and messages are gone.
  p->input.clear();
  p->node(p->token(block(8, 0, .008, .008, 2822400, 2822400), 8, 16));
  REQUIRE(p->output().empty());
  p->node(p->token(block(8, 4, .008, .016, 2822400, 5644800), 8, 16));
  REQUIRE(p->output() == std::vector<MidiEvent>{{0, 0, 60, 0, false}});
  p->node(p->token(block(12, 4, .016, .024, 5644800, 8467200), 12, 16));
  REQUIRE(p->output().empty());
}

TEST_CASE(
    "VelToNote stopped tokens publish only cleanup until resume", "[nodes][veltonote]")
{
  auto p = std::make_unique<Processor>();
  p->node.inputs.end_quant.value = -1.;
  p->input.write_value(60, 0);
  p->node(p->token(block(0, 4, 0., .008, 0, 2822400)));
  p->node.stop();
  p->node(p->token(block(4, 0, .008, .008, 2822400, 2822400), 0, 4));
  REQUIRE(p->output().empty());
  REQUIRE(p->node.needs_service());
  p->node(p->token(block(4, 4, .008, .016, 2822400, 5644800)));
  REQUIRE(p->output() == std::vector<MidiEvent>{{0, 0, 60, 0, false}});
  p->node(p->token(block(8, 4, .016, .024, 5644800, 8467200)));
  REQUIRE(p->output().empty());
  p->node.resume();
  p->node(p->token(block(12, 4, .024, .032, 8467200, 11289600)));
  REQUIRE(p->output() == std::vector<MidiEvent>{{0, 0, 60, 100, true}});
  p->node.pause();
  p->node.cleanup_tick(p->token(block(16, 4, .032, .04, 11289600, 14112000)));
  REQUIRE(p->output() == std::vector<MidiEvent>{{0, 0, 60, 0, false}});
  REQUIRE_FALSE(p->node.needs_service());
}

TEST_CASE(
    "VelToNote duration follows token model and musical spans", "[nodes][veltonote]")
{
  for(bool sync : {false, true})
    for(int tempo : {60, 120, 240})
    {
      INFO("sync=" << sync << " tempo=" << tempo);
      auto p = std::make_unique<Processor>();
      p->node.inputs.fixed_duration.value = true;
      p->node.inputs.duration.sync = sync;
      p->node.inputs.duration.value = sync ? 60.f / tempo : 1.f;
      const int off = (sync ? 60000 : 120000) / tempo;
      const int frames = off + 2;
      const auto model_end = std::int64_t(frames) * 5880 * tempo;
      const double qend = 2. * model_end / flicks_per_second;
      auto tk = p->token(block(0, frames, 0., qend, 0, model_end));
      tk.tempo = tempo;
      p->input.write_value(60, 0);
      p->node(tk);
      REQUIRE(
          p->output()
          == std::vector<MidiEvent>{{0, 0, 60, 100, true}, {off, 0, 60, 0, false}});
    }

  SECTION("actual physical span overrides nominal sample-rate conversion")
  {
    auto p = std::make_unique<Processor>();
    p->node.inputs.fixed_duration.value = true;
    p->node.inputs.duration.value = .5;
    p->input.write_value(60, 10);
    p->node(p->token(block(10, 4, 0., 2., 0, 705600000), 10, 64));
    REQUIRE(
        p->output()
        == std::vector<MidiEvent>{{0, 0, 60, 100, true}, {2, 0, 60, 0, false}});
  }
}

TEST_CASE(
    "VelToNote duration survives tempo change in both travel directions",
    "[nodes][veltonote]")
{
  for(int dir : {-1, 1})
    for(auto unit : {DurationUnit::model_seconds, DurationUnit::quarters})
    {
      INFO("direction=" << dir << " unit=" << static_cast<int>(unit));
      Engine<4, 8> engine;
      Settings s{
          .start_quant = 0.,
          .end_mode = EndMode::duration,
          .duration_unit = unit,
          .duration = unit == DurationUnit::quarters ? 2. : 1.};
      std::vector<MidiEvent> out;
      auto sink = [&](MidiEvent e) { out.push_back(e); };
      engine.push(0, {60, 100});
      engine.process(block(0, 500, 0., dir, 0, dir * 352800000LL), s, sink);
      REQUIRE(out == std::vector<MidiEvent>{{0, 0, 60, 100, true}});
      out.clear();
      engine.process(
          block(500, 250, dir, 2. * dir, dir * 352800000LL, dir * 705600000LL), s, sink);
      REQUIRE(out.empty());
      engine.process(
          block(750, 2, 2. * dir, 2.008 * dir, dir * 705600000LL, dir * 708422400LL), s,
          sink);
      REQUIRE(out == std::vector<MidiEvent>{{0, 0, 60, 0, false}});
    }
}

TEST_CASE(
    "VelToNote grid uses causal smooth grace and reverse odd-meter neighbors",
    "[nodes][veltonote]")
{
  const Grid grid{block(0, 100, 0., 4., 0, 705600000)};
  REQUIRE(onset_target(grid, .4L, .25, 0., 1) == .4L);
  REQUIRE(onset_target(grid, .4L, .25, 1., 1) == 1.L);
  REQUIRE(onset_target(grid, 1.L, .25, 1., 1) == 1.L);
  REQUIRE(onset_target(grid, .25L, .25, .5, 1) == .25L);
  REQUIRE(double(onset_target(grid, .375L, .25, .5, 1)) == Catch::Approx(.53125));
  REQUIRE(double(onset_target(grid, .625L, .25, .5, -1)) == Catch::Approx(.46875));
  const auto left = onset_target(grid, .25L - 1e-6L, .25, .5, 1);
  const auto right = onset_target(grid, .25L + 1e-6L, .25, .5, 1);
  REQUIRE(double(right - left) == Catch::Approx(2e-6).margin(1e-10));

  auto odd = block(0, 100, 0., 7., 0, 705600000);
  odd.numerator = 7;
  odd.denominator = 8;
  odd.bar_end = 7.;
  const Grid odd_grid{odd};
  REQUIRE(odd_grid.next(3.5L, .25, -1, true) == 3.L);
  REQUIRE(odd_grid.next(3.25L, .25, 1) == 3.5L);
  REQUIRE(odd_grid.next(3.25L, .25, -1) == 3.L);
  REQUIRE(
      double(onset_target(odd_grid, 3.1875L, .25, .5, 1)) == Catch::Approx(3.265625));
  REQUIRE(
      double(onset_target(odd_grid, 3.3125L, .25, .5, -1)) == Catch::Approx(3.234375));
}

TEST_CASE(
    "VelToNote absolute events are independent of execution partition",
    "[nodes][veltonote]")
{
  for(auto end_mode : {EndMode::quantized, EndMode::duration})
    for(int dir : {-1, 1})
    {
      auto run = [&](const std::vector<int>& sizes) {
        Engine<16, 32> engine;
        Settings s{
            .start_quant = .125,
            .tightness = .6,
            .end_quant = .25,
            .end_mode = end_mode,
            .duration = .02};
        const std::array<std::pair<int, DecodedNote>, 7> input{
            {{3, {60, 100}},
             {4, {60, 90}},
             {13, {62, 100}},
             {17, {60, 0}},
             {50, {65, 100}},
             {91, {65, 100}},
             {99, {62, 0}}}};
        std::vector<MidiEvent> out;
        int first = 0;
        bool bounded_timestamps = true;
        for(int frames : sizes)
        {
          for(const auto& [date, note] : input)
            if(date >= first && date < first + frames)
              engine.push(date - first, note);
          engine.process(
              block(
                  first, frames, dir * first * .03, dir * (first + frames) * .03,
                  dir * std::int64_t(first) * 705600,
                  dir * std::int64_t(first + frames) * 705600),
              s, [&](MidiEvent e) {
            bounded_timestamps &= e.frame >= 0 && e.frame < frames;
            e.frame += first;
            out.push_back(e);
          });
          first += frames;
        }
        REQUIRE(bounded_timestamps);
        return out;
      };
      const auto whole = run({128});
      REQUIRE(run(std::vector<int>(128, 1)) == whole);
      REQUIRE(run({7, 11, 3, 39, 1, 23, 44}) == whole);
    }

  SECTION("fractional grid deadline does not disappear at a slice boundary")
  {
    Engine<4, 8> engine;
    Settings s{.start_quant = 0., .end_quant = .25};
    std::vector<MidiEvent> out;
    engine.push(0, {60, 100});
    engine.process(
        block(0, 4, 0., 1.2, 0, 2822400), s, [&](MidiEvent e) { out.push_back(e); });
    REQUIRE(out == std::vector<MidiEvent>{{0, 0, 60, 100, true}});
    out.clear();
    engine.process(block(4, 2, 1.2, 1.8, 2822400, 4233600), s, [&](MidiEvent e) {
      out.push_back(e);
    });
    REQUIRE(out == std::vector<MidiEvent>{{0, 0, 60, 0, false}});
  }
}

TEST_CASE(
    "VelToNote transport retires ownership before new starts", "[nodes][veltonote]")
{
  for(int transition : {0, 1, 2, 3})
  {
    Engine<4, 8> engine;
    Settings s{.start_quant = 0., .end_quant = -1.};
    std::vector<MidiEvent> out;
    auto sink = [&](MidiEvent e) { out.push_back(e); };
    engine.push(0, {60, 100});
    engine.process(block(0, 4, 0., 1., 0, 352800000), s, sink);
    out.clear();
    auto next = block(4, 4, 1., 2., 352800000, 705600000);
    if(transition == 0)
      next.discontinuity = true;
    if(transition == 1)
      next = block(4, 4, 0., 1., 0, 352800000); // seek / loop
    if(transition == 2)
      next = block(4, 4, 1., 0., 352800000, 0); // reversal
    if(transition == 3)
      next = block(4, 4, 1., 1., 352800000, 352800000); // stationary
    engine.push(0, {62, 100});
    engine.process(next, s, sink);
    if(transition == 3)
      REQUIRE(out == std::vector<MidiEvent>{{0, 0, 60, 0, false}});
    else
      REQUIRE(
          out == std::vector<MidiEvent>{{0, 0, 60, 0, false}, {0, 0, 62, 100, true}});
  }
}

TEST_CASE(
    "VelToNote random pitch directions preserve captured release identity",
    "[nodes][veltonote]")
{
  for(auto direction :
      {PitchDirection::Both, PitchDirection::Higher, PitchDirection::Lower})
  {
    Engine<4, 8> engine;
    engine.seed(17);
    Settings s{
        .start_quant = 0.,
        .end_quant = -1.,
        .channel = 3,
        .pitch_shift = 12,
        .pitch_random = 12,
        .pitch_direction = direction};
    bool saw_zero = false, saw_positive = false, saw_negative = false;
    for(int i = 0; i < 128; ++i)
    {
      std::vector<MidiEvent> out;
      engine.push(0, {60, 100});
      engine.process(
          block(i * 2, 1, i * 2., i * 2. + 1., i * 2000, i * 2000 + 1000), s,
          [&](MidiEvent e) { out.push_back(e); });
      REQUIRE(out.size() == 1);
      const auto on = out.front();
      REQUIRE(on.on);
      const int offset = int(on.pitch) - 72;
      REQUIRE(offset >= (direction == PitchDirection::Higher ? 0 : -12));
      REQUIRE(offset <= (direction == PitchDirection::Lower ? 0 : 12));
      saw_zero |= offset == 0;
      saw_positive |= offset > 0;
      saw_negative |= offset < 0;
      out.clear();
      auto changed = s;
      changed.channel = 16;
      changed.pitch_shift = -60;
      changed.velocity_random = 127;
      engine.push(0, {60, 0});
      engine.process(
          block(
              i * 2 + 1, 1, i * 2. + 1., i * 2. + 2., i * 2000 + 1000, i * 2000 + 2000),
          changed, [&](MidiEvent e) { out.push_back(e); });
      REQUIRE(out == std::vector<MidiEvent>{{0, on.channel, on.pitch, 0, false}});
    }
    REQUIRE(saw_zero);
    REQUIRE(saw_positive == (direction != PitchDirection::Lower));
    REQUIRE(saw_negative == (direction != PitchDirection::Higher));
  }
}

TEST_CASE(
    "VelToNote rejects malformed numeric input before narrowing", "[nodes][veltonote]")
{
  const ValueDecoder decoder{60, 100};
  REQUIRE_FALSE(ossia::value{std::numeric_limits<float>::infinity()}.apply(decoder));
  REQUIRE_FALSE(ossia::value{std::numeric_limits<float>::quiet_NaN()}.apply(decoder));
  REQUIRE(ossia::value{1e30f}.apply(decoder)->pitch == 127);
  REQUIRE(ossia::value{-1e30f}.apply(decoder)->pitch == 0);
  const ossia::value tiny{std::vector<ossia::value>{60, .00001f, 999}};
  REQUIRE(tiny.apply(decoder)->velocity == 1);
  const ossia::value off{std::vector<ossia::value>{60, 0.f, 999}};
  REQUIRE(off.apply(decoder)->velocity == 0);
  REQUIRE_FALSE(ossia::value{std::vector<ossia::value>{"bad", 100}}.apply(decoder));
}

TEST_CASE("VelToNote overload and refused releases fail closed", "[nodes][veltonote]")
{
  Engine<2, 2> engine;
  Settings s{.start_quant = 0., .end_quant = -1.};
  std::vector<MidiEvent> out;
  auto sink = [&](MidiEvent e) { out.push_back(e); };
  engine.push(0, {60, 100});
  engine.process(block(0, 1, 0., 1., 0, 1000), s, sink);
  out.clear();
  engine.push(0, {61, 100});
  engine.push(0, {62, 100});
  REQUIRE_FALSE(engine.push(0, {60, 0}));
  engine.process(block(1, 1, 1., 2., 1000, 2000), s, [](MidiEvent) { return false; });
  REQUIRE(engine.deferred_release_count() == 1);
  engine.process(block(2, 0, 2., 2., 2000, 2000), s, sink);
  REQUIRE(out.empty());
  REQUIRE(engine.deferred_release_count() == 1);
  engine.push(0, {64, 100});
  engine.process(block(2, 1, 2., 3., 2000, 3000), s, sink);
  REQUIRE(out == std::vector<MidiEvent>{{0, 0, 60, 0, false}, {0, 0, 64, 100, true}});
}

TEST_CASE(
    "VelToNote cancels pending starts and retires stale retrigger deadlines",
    "[nodes][veltonote]")
{
  Engine<4, 8> engine;
  Settings s{.start_quant = .25, .end_mode = EndMode::duration, .duration = .005};
  std::vector<MidiEvent> out;
  auto sink = [&](MidiEvent e) { out.push_back(e); };
  engine.push(1, {60, 100});
  engine.push(2, {60, 0});
  engine.process(block(0, 20, 0., 2., 0, 14112000), s, sink);
  REQUIRE(out.empty());

  // Duration is measured from the grid-delayed on at sample 10, not arrival 1.
  engine.request_reset();
  engine.push(1, {60, 100});
  engine.process(block(0, 20, 0., 2., 0, 14112000), s, sink);
  REQUIRE(out == std::vector<MidiEvent>{{10, 0, 60, 100, true}, {15, 0, 60, 0, false}});
  out.clear();
  engine.request_reset();
  s.start_quant = 0.;
  s.duration = .01;
  engine.push(1, {60, 100});
  engine.push(5, {60, 90});
  engine.process(block(0, 20, 0., 2., 0, 14112000), s, sink);
  REQUIRE(
      out
      == std::vector<MidiEvent>{
          {1, 0, 60, 100, true},
          {5, 0, 60, 0, false},
          {5, 0, 60, 90, true},
          {15, 0, 60, 0, false}});
}

TEST_CASE("VelToNote pitch clipping cannot wrap MIDI bytes", "[nodes][veltonote]")
{
  for(int edge : {0, 127})
  {
    Engine<2, 2> engine;
    Settings s{
        .start_quant = 0.,
        .end_quant = 0.,
        .pitch_shift = edge == 0 ? -127 : 127,
        .pitch_random = 127,
        .pitch_direction = edge == 0 ? PitchDirection::Lower : PitchDirection::Higher};
    std::vector<MidiEvent> out;
    engine.push(0, {static_cast<std::uint8_t>(edge), 100});
    engine.process(
        block(0, 2, 0., 1., 0, 1000), s, [&](MidiEvent e) { out.push_back(e); });
    REQUIRE(
        out
        == std::vector<MidiEvent>{
            {0, 0, static_cast<std::uint8_t>(edge), 100, true},
            {1, 0, static_cast<std::uint8_t>(edge), 0, false}});
  }
}

TEST_CASE(
    "VelToNote voice pool overload never evicts an owned release", "[nodes][veltonote]")
{
  Engine<1, 4> engine;
  Settings s{.start_quant = 0., .end_quant = -1.};
  std::vector<MidiEvent> out;
  engine.push(0, {60, 100});
  engine.push(1, {61, 100});
  engine.push(2, {60, 0});
  engine.process(
      block(0, 3, 0., 1., 0, 1000), s, [&](MidiEvent e) { out.push_back(e); });
  REQUIRE(out == std::vector<MidiEvent>{{0, 0, 60, 100, true}, {2, 0, 60, 0, false}});
  REQUIRE(engine.statistics().dropped_triggers == 1);
}
