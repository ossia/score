#pragma once
#include <Midi/MidiNote.hpp>

#include <Recording/Takes/RecordedNotes.hpp>

#include <score/model/Identifier.hpp>

#include <score_plugin_recording_export.h>

#include <vector>

namespace Midi
{
class ProcessModel;
}
namespace Recording
{
//! What recording notes into a piano roll does to it
struct NoteOverdub
{
  //! The take's notes, in the piano roll's time
  std::vector<Midi::NoteData> add;
  //! Replacing: its notes over the span recorded
  std::vector<Id<Midi::Note>> remove;
};

/**
 * The notes of a take recorded in a box, dates in the box (flicks), as notes
 * of a piano roll of that box: its time is the box's moved by its start
 * offset, round its loop when it loops; a note is a fraction of its length.
 * A note that would cross the loop's end is cut there. Replacing, the notes
 * of the piano roll that sound in [start, end) are removed.
 */
SCORE_PLUGIN_RECORDING_EXPORT
NoteOverdub overdubNotes(
    const Midi::ProcessModel& into, const std::vector<RecordedNote>& notes,
    int64_t start, int64_t end, bool replace);
}
