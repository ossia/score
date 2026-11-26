#include "TakeMidiFile.hpp"

#include <libremidi/writer.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Process
{
std::string takeMidiFile(
    std::vector<TakeMidiEvent> events, int64_t endTick, double tempo, int meterNumerator,
    int meterDenominator)
{
  const double bpm = std::max(1., tempo);
  // A tempo event holds microseconds per quarter on 24 bits.
  const int us_per_quarter
      = int(std::clamp<int64_t>(std::llround(60'000'000. / bpm), 1, 0xFFFFFF));
  events.push_back({0, 1, libremidi::meta_events::tempo(us_per_quarter)});
  // 24 MIDI clocks per metronome click and 8 thirty-seconds per quarter are
  // what every sequencer writes.
  events.push_back(
      {0, 1, libremidi::meta_events::time_signature(
                 std::clamp(meterNumerator, 1, 255), std::clamp(meterDenominator, 1, 128),
                 24, 8)});
  events.push_back({endTick, 3, libremidi::message{0xFF, 0x06, 3, 'e', 'n', 'd'}});

  std::stable_sort(
      events.begin(), events.end(), [](const TakeMidiEvent& a, const TakeMidiEvent& b) {
    return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
  });

  libremidi::writer w;
  w.ticksPerQuarterNote = takeTicksPerQuarter;
  int64_t prev = 0;
  for(const auto& e : events)
  {
    w.add_event(int(e.tick - prev), 0, e.message);
    prev = e.tick;
  }
  std::ostringstream out;
  w.write(out);
  return std::move(out).str();
}
}
