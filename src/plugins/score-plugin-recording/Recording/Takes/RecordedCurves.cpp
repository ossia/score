#include "RecordedCurves.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>

#include <Automation/AutomationModel.hpp>

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

std::optional<RecordedCurve> spliceCurve(
    const Automation::ProcessModel& into, const std::vector<ValueTake::Event>& events,
    int component, int64_t start, int64_t end, CurveOptions options)
{
  const double duration = double(into.duration().impl);
  if(duration <= 0. || end <= start || component < 0)
    return std::nullopt;
  const bool loops = into.loops() && into.loopDuration().impl > 0;
  const int64_t loop = into.loopDuration().impl;
  const int64_t offset = into.startOffset().impl;
  const auto x_of = [&](int64_t date) {
    if(loops)
      date = ((date % loop) + loop) % loop;
    return std::clamp(double(date + offset) / duration, 0., 1.);
  };

  // The recorded values, in the automation's time
  std::vector<std::pair<double, double>> rec;
  for(const auto& e : events)
  {
    const auto n = numbersOf(e.value);
    if(!n || int(n->size()) <= component)
      continue;
    rec.emplace_back(x_of(std::clamp(e.date, start, end)), (*n)[component]);
  }
  if(rec.empty())
    return std::nullopt;
  double x0 = x_of(start), x1 = x_of(end);
  if(loops && x1 <= x0)
    x1 = std::min(1., double(loop + offset) / duration);

  // The automation's curve, as values
  const double lo = into.min(), hi = into.max();
  const auto value = [&](double y) { return lo + y * (hi - lo); };
  const auto& segs = into.curve().sortedSegments();
  const auto existing_at = [&](double x) {
    for(auto* seg : segs)
      if(x >= seg->start().x() && x <= seg->end().x())
        return value(seg->valueAt(x));
    if(!segs.empty())
      return x < segs.front()->start().x() ? value(segs.front()->start().y())
                                           : value(segs.back()->end().y());
    return lo;
  };
  std::vector<std::pair<double, double>> before, after;
  for(auto* seg : segs)
  {
    constexpr int steps = 16;
    const double sx = seg->start().x(), ex = seg->end().x();
    for(int i = 0; i <= steps; i++)
    {
      const double x = sx + (ex - sx) * i / steps;
      const double v = i == 0 ? value(seg->start().y())
                       : i == steps ? value(seg->end().y())
                                    : value(seg->valueAt(x));
      if(x < x0)
        before.emplace_back(x, v);
      else if(x > x1)
        after.emplace_back(x, v);
    }
  }

  std::vector<std::pair<double, double>> pts = std::move(before);
  pts.emplace_back(x0, existing_at(x0));
  // The values, from the start of the span to its end
  double held = rec.front().second;
  pts.emplace_back(x0, held);
  for(const auto& [x, v] : rec)
  {
    if(options.stepped && x > pts.back().first)
      pts.emplace_back(x, held);
    pts.emplace_back(x, v);
    held = v;
  }
  pts.emplace_back(x1, held);
  pts.emplace_back(x1, existing_at(x1));
  for(auto& p : after)
    pts.push_back(p);
  // An automation's curve goes from 0 to 1
  if(pts.front().first > 0.)
    pts.insert(pts.begin(), {0., existing_at(0.)});
  if(pts.back().first < 1.)
    pts.emplace_back(1., pts.back().second);

  RecordedCurve c;
  c.min = std::min(lo, hi);
  c.max = std::max(lo, hi);
  for(auto& [x, v] : pts)
  {
    c.min = std::min(c.min, v);
    c.max = std::max(c.max, v);
  }
  const double span = c.max > c.min ? c.max - c.min : 1.;
  for(std::size_t i = 0; i + 1 < pts.size(); i++)
  {
    // Vertical jumps are kept; points at the same place are one
    if(pts[i + 1].first == pts[i].first && pts[i + 1].second == pts[i].second)
      continue;
    Curve::SegmentData d;
    d.id = Id<Curve::SegmentModel>{int(c.segments.size())};
    d.start = {pts[i].first, (pts[i].second - c.min) / span};
    d.end = {pts[i + 1].first, (pts[i + 1].second - c.min) / span};
    if(!c.segments.empty())
    {
      d.previous = c.segments.back().id;
      c.segments.back().following = d.id;
    }
    d.type = Metadata<ConcreteKey_k, Curve::LinearSegment>::get();
    d.specificSegmentData = QVariant::fromValue(Curve::LinearSegmentData{});
    c.segments.push_back(std::move(d));
  }
  if(c.segments.empty())
    return std::nullopt;
  return c;
}
}
