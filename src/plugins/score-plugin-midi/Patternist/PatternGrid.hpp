#pragma once
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>

#include <ossia/editor/scenario/time_signature.hpp>

#include <Patternist/PatternModel.hpp>

#include <score_plugin_midi_export.h>

#include <utility>
#include <vector>

namespace Patternist
{
//! The bar grid a pattern plays on, in the time base of the interval whose
//! musical positions the engine reads: the closest one, from the layer's own
//! upwards, with a time signature or a tempo curve.
struct TimeGrid
{
  //! Date of the layer's left edge.
  TimeVal origin{};
  //! Date the process starts playing at: its first step is the first grid
  //! point from there on.
  TimeVal playStart{};
  //! Sorted by date. Empty, or before the first entry, is 4/4 from 0.
  std::vector<std::pair<TimeVal, ossia::time_signature>> signatures;

  bool operator==(const TimeGrid&) const noexcept = default;
};

//! One step as it falls on the timeline.
struct StepSlot
{
  double x0{};
  double x1{};
  int step{};
  //! Past the first pass through the pattern, or before the process starts.
  bool repeat{};
  bool barStart{};
  bool beatStart{};
  //! Index of the beat the step falls in, counted from its bar line.
  int beat{};
  //! Steps between the first one and this one: negative before the process
  //! starts.
  int64_t index{};
  //! Which of two tones the cell takes: beats alternate, or bars when a step
  //! lasts a beat or more.
  int shade{};
};

//! The steps that fall in [0; width[ pixels, on the grid the engine walks:
//! `division` points per whole note, restarting at every bar line, the steps
//! looping every `length`. Empty when there would be more than `maxSlots`.
SCORE_PLUGIN_MIDI_TEST_EXPORT
std::vector<StepSlot> timelineSlots(
    int length, int division, const TimeGrid& grid, ZoomRatio zoom, double width,
    std::size_t maxSlots = 100000);

inline std::vector<StepSlot> timelineSlots(
    const Pattern& p, const TimeGrid& grid, ZoomRatio zoom, double width,
    std::size_t maxSlots = 100000)
{
  return timelineSlots(p.length, p.division, grid, zoom, width, maxSlots);
}

//! The signature in effect at `date`.
SCORE_PLUGIN_MIDI_EXPORT
ossia::time_signature signatureAt(const TimeGrid& grid, TimeVal date) noexcept;

//! The steps of a pattern spread evenly over [x0; x1[, grouped in beats and
//! bars of `sig` counted from the first step.
SCORE_PLUGIN_MIDI_EXPORT
std::vector<StepSlot>
fitSlots(const Pattern& p, ossia::time_signature sig, double x0, double x1);

//! Musical length, in quarters, of the longest loop among the lanes.
SCORE_PLUGIN_MIDI_EXPORT double longestLoop(const Pattern& p) noexcept;

//! The steps of one lane over [x0; x1[, where x1 - x0 is `span` quarters: a
//! lane with a shorter loop repeats, a slower one has wider steps, so that the
//! lanes line up in time.
SCORE_PLUGIN_MIDI_TEST_EXPORT
std::vector<StepSlot> fitLaneSlots(
    int length, int division, ossia::time_signature sig, double span, double x0,
    double x1);

//! Every lane of a pattern over [x0; x1[.
struct FitLayout
{
  std::vector<std::vector<StepSlot>> lanes;
  //! Quarters across the width when the lanes line up, 0 otherwise.
  double span{};
};

//! The lanes line up in time, as long as every step stays at least `minStep`
//! wide: past that, each lane spreads its own loop over the whole width.
SCORE_PLUGIN_MIDI_TEST_EXPORT
FitLayout fitLayout(
    const Pattern& p, ossia::time_signature sig, double x0, double x1, double minStep);

//! Position in `slots` of the step a lane plays, given how many steps it played
//! before it, or -1. On repeats, the one the time falls on when it shows that
//! step.
SCORE_PLUGIN_MIDI_TEST_EXPORT
int playingSlot(
    const std::vector<StepSlot>& slots, int64_t played, int length, int division,
    double span) noexcept;

//! Position in `slots` of the slot with that index, or -1.
SCORE_PLUGIN_MIDI_TEST_EXPORT
int slotWithIndex(const std::vector<StepSlot>& slots, int64_t index) noexcept;
}
