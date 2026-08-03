// Cross-implementation parity: ossia::token_request's quantification grid and
// halp::tick_musical's are documented as behaviourally identical - a native
// node and an avendish plug-in on the same score must snap to the same
// samples. This drives both with the token streams time_interval emits
// (floor + carried-residue advance) and compares:
//
//  - exactly-once delivery: over consecutive ticks, each implementation must
//    report every musical grid point exactly once ([start; end[ ownership);
//  - the metronome grids, which agree exactly;
//  - the quantification points, which agree on count and index everywhere,
//    and on the frame within one sample. Exact frame equality does not hold:
//    ossia truncates the musical position to a whole-flick date and the
//    consumer floors that into frames, halp floors the musical position into
//    frames directly, and on ~2e-5 of ticks the two land one frame apart.
//    That residual divergence is pinned by the [!mayfail] case at the end so
//    a change in either direction is visible.

#include <ossia/dataflow/token_request.hpp>

#include <halp/audio.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <map>
#include <vector>

namespace
{
constexpr double Q = 352800000.0; // flicks per quarter through model time
constexpr double FPS48 = 705600000.0 / 48000.0;

struct both_ticks
{
  ossia::token_request tok;
  halp::tick_musical hm;
};

both_ticks make_tick(int64_t prev, int64_t date, int L, int sn, int sd)
{
  both_ticks r;
  auto& tok = r.tok;
  tok.prev_date = ossia::time_value{prev};
  tok.date = ossia::time_value{date};
  tok.speed = date >= prev ? 1.0 : -1.0;
  tok.tempo = 120.;
  tok.signature = ossia::time_signature{(uint16_t)sn, (uint16_t)sd};
  tok.start_sample = 0;
  tok.length_sample = L;

  const double qib = 4. * sn / sd;
  const auto bar_of = [&](double pos) { return std::floor(pos / qib) * qib; };
  tok.musical_start_last_signature = 0.;
  tok.musical_start_position = prev / Q;
  tok.musical_start_last_bar = bar_of(tok.musical_start_position);
  tok.musical_end_position = date / Q;
  tok.musical_end_last_bar = bar_of(tok.musical_end_position);

  auto& hm = r.hm;
  hm.frames = L;
  hm.tempo = 120.;
  hm.signature = {sn, sd};
  hm.start_position_in_quarters = tok.musical_start_position;
  hm.end_position_in_quarters = tok.musical_end_position;
  hm.last_signature_change = 0.;
  hm.bar_at_start = tok.musical_start_last_bar;
  hm.bar_at_end = tok.musical_end_last_bar;
  return r;
}

template <typename F>
void for_each_production_tick(int sn, int sd, double speed, int L, F&& f)
{
  double residue = 0.;
  int64_t d = 0;
  for(int t = 0; t < 4000; t++)
  {
    const double exact = L * FPS48 * speed + residue;
    const double step = std::floor(exact);
    residue = exact - step;
    const int64_t nd = d + int64_t(step);
    f(make_tick(d, nd, L, sn, sd));
    d = nd;
  }
}

const int sigs[][2] = {{4, 4}, {3, 4}, {7, 8}, {6, 8}, {5, 4}};
const double rates[] = {0.5, 1., 1.5, 2., 3., 4., 8., 16.};
const double speeds[] = {1.0, 0.5, 1.37, 2.0};
}

TEST_CASE(
    "quantification: ossia and halp both deliver each grid point exactly once",
    "[quantification][parity]")
{
  for(auto [sn, sd] : sigs)
    for(double rate : {1., 1.5, 3., 8.})
      for(double speed : speeds)
      {
        std::map<long long, int> ossia_seen, halp_seen;
        double last_end = 0;
        for_each_production_tick(sn, sd, speed, 512, [&](const both_ticks& bt) {
          const double s = bt.tok.musical_start_position;
          const double e = bt.tok.musical_end_position;
          for(const auto& p : bt.tok.get_quantification_dates(rate))
          {
            const double frac = double((p.date - bt.tok.prev_date).impl)
                                / double((bt.tok.date - bt.tok.prev_date).impl);
            ossia_seen[llround((s + frac * (e - s)) * 24.)]++;
          }
          for(const auto& p : bt.hm.get_quantification_date(rate))
          {
            const double frac = double(p.first) / double(bt.hm.frames);
            halp_seen[llround((s + frac * (e - s)) * 24.)]++;
          }
          last_end = e;
        });

        const double qib = 4. * sn / sd;
        for(const auto& [key, count] : ossia_seen)
          if(key / 24. < last_end - qib)
          {
            INFO(
                "ossia: sig " << sn << "/" << sd << " rate " << rate << " speed "
                              << speed << " point " << key / 24. << " fired "
                              << count << " times");
            REQUIRE(count == 1);
          }
        for(const auto& [key, count] : halp_seen)
          if(key / 24. < last_end - qib)
          {
            INFO(
                "halp: sig " << sn << "/" << sd << " rate " << rate << " speed "
                             << speed << " point " << key / 24. << " fired "
                             << count << " times");
            REQUIRE(count == 1);
          }
      }
}

TEST_CASE(
    "quantification: ossia and halp metronomes agree exactly",
    "[quantification][parity]")
{
  const double ratio = 1.0 / FPS48;
  int compared = 0;
  for(auto [sn, sd] : sigs)
    for(double speed : speeds)
      for(int L : {128, 512})
        for_each_production_tick(sn, sd, speed, L, [&](const both_ticks& bt) {
          std::vector<std::pair<int64_t, int>> o, h;
          bt.tok.metronome(
              ratio, [&](int64_t s) { o.push_back({s, 1}); },
              [&](int64_t s) { o.push_back({s, 0}); });
          bt.hm.metronome(
              [&](int64_t s) { h.push_back({s, 1}); },
              [&](int64_t s) { h.push_back({s, 0}); });
          INFO(
              "sig " << sn << "/" << sd << " speed " << speed << " prev "
                     << bt.tok.prev_date.impl << " date " << bt.tok.date.impl);
          REQUIRE(o == h);
          compared++;
        });
  CHECK(compared > 0);
}

TEST_CASE(
    "quantification: ossia and halp points agree within one frame",
    "[quantification][parity]")
{
  const double ratio = 1.0 / FPS48;
  for(auto [sn, sd] : sigs)
    for(double rate : rates)
      for(double speed : speeds)
        for_each_production_tick(sn, sd, speed, 512, [&](const both_ticks& bt) {
          const auto op = bt.tok.get_quantification_dates(rate);
          const auto hp = bt.hm.get_quantification_date(rate);
          INFO(
              "sig " << sn << "/" << sd << " rate " << rate << " speed " << speed
                     << " prev " << bt.tok.prev_date.impl << " date "
                     << bt.tok.date.impl);
          REQUIRE(op.size() == hp.size());
          for(std::size_t i = 0; i < op.size(); i++)
          {
            REQUIRE(op[i].index == hp[i].second);
            const auto of = bt.tok.to_physical_time_in_tick(op[i].date, ratio);
            REQUIRE(std::abs(double(of - hp[i].first)) <= 1.);
          }
        });
}

// The residual divergence, pinned: with 512-sample ticks over these
// signatures, rates and speeds, ossia and halp currently disagree by one
// frame on a small number of points (the flick-truncation double rounding).
// This case states the ideal - exact agreement - and is expected to fail
// until the two implementations round the same way; if it starts passing,
// promote it to a strict test.
TEST_CASE(
    "quantification: ossia and halp points agree exactly",
    "[quantification][parity][!mayfail]")
{
  const double ratio = 1.0 / FPS48;
  int mismatches = 0;
  for(auto [sn, sd] : sigs)
    for(double rate : rates)
      for(double speed : speeds)
        for_each_production_tick(sn, sd, speed, 448, [&](const both_ticks& bt) {
          const auto op = bt.tok.get_quantification_dates(rate);
          const auto hp = bt.hm.get_quantification_date(rate);
          if(op.size() != hp.size())
          {
            mismatches++;
            return;
          }
          for(std::size_t i = 0; i < op.size(); i++)
          {
            const auto of = bt.tok.to_physical_time_in_tick(op[i].date, ratio);
            if(of != hp[i].first || op[i].index != hp[i].second)
              mismatches++;
          }
        });
  CHECK(mismatches == 0);
}
