#pragma once
#include <Patternist/PatternModel.hpp>

#include <score_plugin_midi_export.h>

#include <optional>
#include <vector>

class QByteArray;
class QMimeData;
namespace Patternist
{
//! A note of a MIDI file, in quarter notes from the start of the file.
struct MidiNote
{
  double start{};
  double duration{};
  uint8_t pitch{};
  //! 1 - 127.
  uint8_t velocity{100};
  //! On the drum channel: how long it lasts says nothing.
  bool drum{};
};

struct MidiClip
{
  std::vector<MidiNote> notes;
  //! Quarter notes from the start of the file to the end of its longest track.
  double length{};
};

//! The notes of a Standard MIDI file, every track and channel together. Empty
//! when the data is not one.
SCORE_PLUGIN_MIDI_TEST_EXPORT MidiClip readMidiFile(const QByteArray& data) noexcept;

//! The first MIDI file a drop carries that has notes.
SCORE_PLUGIN_MIDI_TEST_EXPORT std::optional<MidiClip> midiFromMime(const QMimeData& mime);

struct MidiMatch
{
  Pattern pattern;
  //! Steps written.
  int notes{};
  //! Notes that fell on a step another one of the same pitch took.
  int merged{};
  //! Notes past the end of the lane they go to, or whose pitch found no room
  //! for a lane.
  int dropped{};
};

//! Writes a clip over the steps of a pattern, on the grid the pattern has:
//! the pattern's length and division, and the length and rate of each lane,
//! stay as they are.
//!
//! - A lane per pitch: the lanes playing a pitch of the clip get its notes, the
//!   other pitches get new lanes, which follow the pattern's length and rate,
//!   placed among the note lanes from the high notes down. Every note lane
//!   loses the steps it had; the accent and slide lanes are left alone.
//! - Each note goes to the step of its lane nearest its start; how far off it
//!   was is kept as the step's offset, so that swung or tuplet rhythms play as
//!   written. Of two notes of a pitch on one step, the one nearest the grid
//!   stays. What starts past the end of its lane is left out.
//! - Velocities are kept: the loudest note of a lane sets the lane's, the
//!   steps are relative to it.
//! - Off the drum channel, a note shorter than its step gets a gate and a
//!   longer one is tied over the steps it covers.
//!
//! A clip without notes leaves the pattern as it is.
SCORE_PLUGIN_MIDI_TEST_EXPORT MidiMatch matchToGrid(const Pattern& current, const MidiClip& clip);
}
