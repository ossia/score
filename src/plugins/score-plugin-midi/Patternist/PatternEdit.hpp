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

//! Fills lane `lane`, or every lane when it is -1, with notes at random, more
//! of them on the beats.
SCORE_PLUGIN_MIDI_EXPORT void
randomize(Pattern& p, std::mt19937& rng, int lane = -1) noexcept;

//! How generated steps combine with those already written.
enum class Combine : uint8_t
{
  Replace,
  //! Keeps the written notes and adds the generated ones.
  Add,
  //! Keeps the written notes that fall on a generated one.
  Mask
};

//! Writes `hits` notes spread as evenly as possible over the steps the lane
//! plays, turned `rotation` steps later.
SCORE_PLUGIN_MIDI_EXPORT void euclidean(
    const Pattern& p, Lane& l, int hits, int rotation, Combine c = Combine::Replace) noexcept;

SCORE_PLUGIN_MIDI_EXPORT void addLane(Pattern& p);
SCORE_PLUGIN_MIDI_EXPORT void removeLane(Pattern& p, int lane);
SCORE_PLUGIN_MIDI_EXPORT void setLength(Pattern& p, int length);

SCORE_PLUGIN_MIDI_EXPORT
QMimeData* patternToMime(const Pattern& p);
SCORE_PLUGIN_MIDI_EXPORT
std::optional<Pattern> patternFromMime(const QMimeData& mime);

//! A rectangle of steps: lanes `lane0` to `lane1` and steps `step0` to `step1`,
//! all included.
struct StepRange
{
  int lane0{-1}, lane1{-1};
  int step0{-1}, step1{-1};

  bool valid() const noexcept { return lane0 >= 0 && step0 >= 0; }
  bool contains(int lane, int step) const noexcept
  {
    return valid() && lane >= lane0 && lane <= lane1 && step >= step0 && step <= step1;
  }
  bool operator==(const StepRange&) const noexcept = default;
};

//! The steps of a range, notes and parameters, as a pattern of as many lanes
//! and steps; their notes are left at 0.
SCORE_PLUGIN_MIDI_EXPORT Pattern copySteps(const Pattern& p, StepRange r);
//! Writes copied steps with their first lane on `lane` and their first step on
//! `step`; what falls past the last lane or a lane's length is dropped.
SCORE_PLUGIN_MIDI_EXPORT void pasteSteps(Pattern& p, const Pattern& steps, int lane, int step);
SCORE_PLUGIN_MIDI_EXPORT void clearSteps(Pattern& p, StepRange r) noexcept;
SCORE_PLUGIN_MIDI_EXPORT QMimeData* stepsToMime(const Pattern& steps);
SCORE_PLUGIN_MIDI_EXPORT std::optional<Pattern> stepsFromMime(const QMimeData& mime);

//! The step parameters the editor draws as bars.
enum class StepField : uint8_t
{
  Velocity,
  Chance,
  Offset,
  Gate,
  Ratchet,
  Pitch
};

struct FieldRange
{
  float min, max;
  //! Bars start from this value: 0 for most, the middle for offsets and pitch.
  float origin;
};
SCORE_PLUGIN_MIDI_EXPORT FieldRange fieldRange(StepField f) noexcept;
SCORE_PLUGIN_MIDI_EXPORT QString fieldName(StepField f);
SCORE_PLUGIN_MIDI_EXPORT float stepField(const StepParams& p, StepField f) noexcept;
SCORE_PLUGIN_MIDI_EXPORT void setStepField(StepParams& p, StepField f, float v) noexcept;
//! The value as the editor writes it next to its bar.
SCORE_PLUGIN_MIDI_EXPORT QString fieldText(StepField f, float v);

//! Sets one parameter of the steps of `lane`, or of every lane when it is -1,
//! to random values in a musical range: humanized velocities and timings,
//! mostly single hits with a few ratchets.
SCORE_PLUGIN_MIDI_EXPORT void
randomizeField(Pattern& p, std::mt19937& rng, StepField f, int lane = -1) noexcept;
//! Puts one parameter back to its default.
SCORE_PLUGIN_MIDI_EXPORT void resetField(Pattern& p, StepField f, int lane = -1) noexcept;

//! What a lane header shows for a note.
SCORE_PLUGIN_MIDI_EXPORT QString laneLabel(int note, LaneLabels mode);
}
