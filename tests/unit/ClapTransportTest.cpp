// Clap::make_transport — the tick -> clap_event_transport_t conversion.
//
// Regression: CLAP song positions are fixed-point (clap_beattime/clap_sectime
// are int64 scaled by 1<<31, clap/fixedpoint.h). The old code cast raw beat
// counts (and floored away the fractional beat), so hosts reading
// song_pos_beats / CLAP_BEATTIME_FACTOR saw the transport frozen at ~0:
// transport-following plug-ins (the Stochas step sequencer, arpeggiators,
// synced delays) never advanced even though IS_PLAYING was set.

#include <Clap/Transport.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
ossia::token_request tick(double beats_start, double beats_end, double tempo)
{
  ossia::token_request tk{};
  tk.prev_date = ossia::time_value{0};
  tk.date = ossia::time_value{1000};
  tk.musical_start_position = beats_start;
  tk.musical_end_position = beats_end;
  tk.musical_start_last_bar = 4.0 * (int)(beats_start / 4.0);
  tk.tempo = tempo;
  tk.signature = {4, 4};
  return tk;
}

double to_beats(clap_beattime t)
{
  return (double)t / CLAP_BEATTIME_FACTOR;
}
double to_seconds(clap_sectime t)
{
  return (double)t / CLAP_SECTIME_FACTOR;
}
}

TEST_CASE("clap transport: song position is fixed-point beats", "[clap][transport]")
{
  // 6.5 quarter notes in: the fractional part must survive
  const auto t = Clap::make_transport(tick(6.5, 6.6, 120.));

  CHECK(to_beats(t.song_pos_beats) == Catch::Approx(6.5));
  CHECK(to_beats(t.bar_start) == Catch::Approx(4.0));
  CHECK(t.bar_number == 1);
  CHECK(t.tempo == 120.);
  CHECK(t.tsig_num == 4);
  CHECK(t.tsig_denom == 4);

  // 6.5 quarters at 120 BPM = 3.25 s
  CHECK(to_seconds(t.song_pos_seconds) == Catch::Approx(3.25));

  CHECK(t.flags & CLAP_TRANSPORT_IS_PLAYING);
  CHECK(t.flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE);
  CHECK(t.flags & CLAP_TRANSPORT_HAS_SECONDS_TIMELINE);
  CHECK(t.flags & CLAP_TRANSPORT_HAS_TEMPO);
  CHECK(t.flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE);
}

TEST_CASE("clap transport: position advances tick after tick", "[clap][transport]")
{
  // The actual Stochas symptom: consecutive ticks must yield strictly
  // increasing song positions (the old cast-to-int64 made them all ~0)
  clap_beattime prev = -1;
  for(double b = 0.; b < 8.; b += 0.25)
  {
    const auto t = Clap::make_transport(tick(b, b + 0.25, 132.));
    CHECK(t.song_pos_beats > prev);
    prev = t.song_pos_beats;
  }
  // After 31 ticks of a quarter beat, we are at 7.75 beats, not at ~0
  CHECK(to_beats(prev) == Catch::Approx(7.75));
}

TEST_CASE("clap transport: paused tick is not playing", "[clap][transport]")
{
  auto tk = tick(2.0, 2.0, 120.);
  tk.date = tk.prev_date; // zero-length tick: transport paused
  const auto t = Clap::make_transport(tk);
  CHECK(!(t.flags & CLAP_TRANSPORT_IS_PLAYING));
}

TEST_CASE("clap transport: 6/8 bar math", "[clap][transport]")
{
  auto tk = tick(9.0, 9.1, 90.);
  tk.signature = {6, 8};
  tk.musical_start_last_bar = 6.0; // bars are 3 quarters long in 6/8
  const auto t = Clap::make_transport(tk);
  CHECK(to_beats(t.bar_start) == Catch::Approx(6.0));
  CHECK(t.bar_number == 2);
  CHECK(t.tsig_num == 6);
  CHECK(t.tsig_denom == 8);
}
