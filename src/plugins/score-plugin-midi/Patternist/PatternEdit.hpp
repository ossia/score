#pragma once
#include <Patternist/PatternModel.hpp>

#include <score_plugin_midi_export.h>

#include <optional>
#include <random>

class QMimeData;
namespace Patternist
{
//! Operations on one pattern, shared by the layer and the editor. They work on
//! a copy that the caller then commits with UpdatePattern.

//! The state a click on a step gives it: a note on a rest, a rest otherwise.
SCORE_PLUGIN_MIDI_EXPORT Note toggledNote(Note current) noexcept;

SCORE_PLUGIN_MIDI_EXPORT bool setStep(Pattern& p, int lane, int step, Note n) noexcept;

SCORE_PLUGIN_MIDI_EXPORT void clear(Pattern& p) noexcept;

//! Moves the first `length` steps of every lane by `steps`, wrapping around;
//! positive moves them later.
SCORE_PLUGIN_MIDI_EXPORT void rotate(Pattern& p, int steps) noexcept;

//! Fills every lane with notes at random, more of them on the beats.
SCORE_PLUGIN_MIDI_EXPORT void randomize(Pattern& p, std::mt19937& rng) noexcept;

SCORE_PLUGIN_MIDI_EXPORT void addLane(Pattern& p);
SCORE_PLUGIN_MIDI_EXPORT void removeLane(Pattern& p, int lane);
SCORE_PLUGIN_MIDI_EXPORT void setLength(Pattern& p, int length);

SCORE_PLUGIN_MIDI_EXPORT
QMimeData* patternToMime(const Pattern& p);
SCORE_PLUGIN_MIDI_EXPORT
std::optional<Pattern> patternFromMime(const QMimeData& mime);

//! What a lane header shows for a note.
SCORE_PLUGIN_MIDI_EXPORT QString laneLabel(int note, LaneLabels mode);
}
