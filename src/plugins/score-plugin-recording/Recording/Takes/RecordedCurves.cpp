#include "RecordedCurves.hpp"

#include <Curve/Segment/PointArray/PointArraySegment.hpp>

#include <Recording/Takes/RecordedValues.hpp>

#include <algorithm>
#include <memory>

namespace Recording
{
std::optional<RecordedCurve> curveOf(
    const std::vector<ValueTake::Event>& events, int component, int64_t start,
    int64_t end, CurveOptions options)
{
  if(end <= start || component < 0)
    return std::nullopt;
  constexpr double flicks_per_msec = 705600.;
  const double length = double(end - start) / flicks_per_msec;

  // The points in milliseconds from the start
  std::vector<std::pair<double, double>> points;
  points.reserve(events.size());
  for(const auto& e : events)
  {
    const auto n = numbersOf(e.value);
    if(!n || int(n->size()) <= component)
      continue;
    const double x = std::clamp(double(e.date - start) / flicks_per_msec, 0., length);
    points.emplace_back(x, (*n)[component]);
  }
  if(points.empty())
    return std::nullopt;

  Curve::SegmentData data;
  data.id = Id<Curve::SegmentModel>{0};
  data.start = {0., 0.};
  data.end = {1., 1.};
  data.specificSegmentData = QVariant::fromValue(Curve::PointArraySegmentData{
      0., length, points.front().second, points.front().second,
      std::make_shared<Curve::PointArraySamples>()});
  Curve::PointArraySegment seg{data, nullptr};
  seg.reserve(points.size() * 2 + 2);

  // The first value from the start
  seg.addPoint(0., points.front().second);
  double prev_x = 0., prev_y = points.front().second;
  for(const auto& [x, y] : points)
  {
    // A value holds until just before the next one
    if(options.stepped && x > prev_x)
      seg.addPoint(x - std::min(1., (x - prev_x) / 2.), prev_y);
    seg.addPoint(x, y);
    prev_x = x;
    prev_y = y;
  }
  // The last one up to the end
  if(length > prev_x)
    seg.addPoint(length, prev_y);

  if(options.simplify && options.simplificationRatio > 0.)
    seg.simplify(options.simplificationRatio);

  RecordedCurve c;
  c.min = seg.min();
  c.max = seg.max();
  c.segments = seg.toPowerSegments();
  if(c.segments.empty())
    return std::nullopt;
  return c;
}
}
