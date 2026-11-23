#pragma once
#include <Curve/Segment/CurveSegmentData.hpp>

#include <Recording/Engine/ValueRecorder.hpp>

#include <score_plugin_recording_export.h>

#include <optional>
#include <vector>

namespace Recording
{
//! An automation's curve: its segments from 0 to 1, between min and max
struct RecordedCurve
{
  double min{};
  double max{};
  std::vector<Curve::SegmentData> segments;
};

struct CurveOptions
{
  //! Each value holds until the next (a parameter); else the curve goes
  //! from one value to the next (messages)
  bool stepped{true};
  //! Fewer points, as the Pen tool does, with the Recording settings' ratio
  bool simplify{true};
  double simplificationRatio{10.};
};

/**
 * The curve that one number of recorded values draws over [start, end] of a
 * box (dates in flicks): the first value holds from the start up to it, the
 * last one up to the end. Nothing when no value has that number.
 */
SCORE_PLUGIN_RECORDING_EXPORT
std::optional<RecordedCurve> curveOf(
    const std::vector<ValueTake::Event>& events, int component, int64_t start,
    int64_t end, CurveOptions options);
}
