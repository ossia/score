#pragma once
#include <Recording/Engine/MidiRecorder.hpp>

#include <score_plugin_recording_export.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Recording
{
//! A note of a MIDI take, dated in the box, in flicks
struct RecordedNote
{
  int64_t start{};
  int64_t duration{};
  uint8_t pitch{};
  uint8_t velocity{};
  uint8_t channel{};

  bool operator==(const RecordedNote&) const noexcept = default;
};

/**
 * The notes the events of a take make: each note on with the note off of the
 * same channel and pitch that follows it (a note on of velocity 0 is a note
 * off, in MIDI 1 and as it comes translated to MIDI 2). A note still held at the end of the take ends there; a note off
 * with no note on before it is ignored. Sorted by start date.
 */
SCORE_PLUGIN_RECORDING_EXPORT
std::vector<RecordedNote>
notesOf(const std::vector<MidiTake::Event>& events, int64_t endDate);

//! Moves the start of each note to the nearest multiple of `quarters` quarter
//! notes (box dates, flicks: a quarter is 352800000); 0 leaves them
SCORE_PLUGIN_RECORDING_EXPORT
void quantizeNotes(std::vector<RecordedNote>& notes, double quarters);

/**
 * A Standard MIDI File of the notes, dated from `start` (flicks), the file
 * lasting `length`. Ticks count quarters at `tempo`, which is how score places
 * a MIDI file: by its quarters, in the tempo of where it is dropped.
 */
SCORE_PLUGIN_RECORDING_EXPORT
std::string smfOf(
    const std::vector<RecordedNote>& notes, int64_t start, int64_t length, double tempo,
    int meterNumerator = 4, int meterDenominator = 4);
}
