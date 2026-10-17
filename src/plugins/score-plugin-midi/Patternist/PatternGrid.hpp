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
};

//! The steps that fall in [0; width[ pixels, on the grid the engine walks:
//! `division` points per whole note, restarting at every bar line, the steps
//! looping every `length`. Empty when there would be more than `maxSlots`.
SCORE_PLUGIN_MIDI_EXPORT
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
SCORE_PLUGIN_MIDI_EXPORT
std::vector<StepSlot> fitLaneSlots(
    int length, int division, ossia::time_signature sig, double span, double x0,
    double x1);
}
