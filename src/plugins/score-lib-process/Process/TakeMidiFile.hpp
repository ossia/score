#pragma once
#include <libremidi/message.hpp>

#include <score_lib_process_export.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Process
{
//! Ticks in a quarter note of the MIDI files takes are written to
inline constexpr int takeTicksPerQuarter = 480;

//! An event of a MIDI take, in ticks from its start
struct TakeMidiEvent
{
  int64_t tick{};
  //! Among events of a tick: note-offs (0), then the others (1), then
  //! note-ons (2)
  int order{1};
  libremidi::message message;
};

/**
 * A take as a Standard MIDI File, at the tempo and in the meter it was played
 * in. Ticks count quarters, so the tempo only says how fast they went: score
 * places a file by its quarters. A marker at `endTick` makes the track end
 * where the take ends, even when its last note ends earlier. Shared by the
 * loopers and the recordings.
 */
SCORE_LIB_PROCESS_EXPORT std::string takeMidiFile(
    std::vector<TakeMidiEvent> events, int64_t endTick, double tempo,
    int meterNumerator, int meterDenominator);
}
