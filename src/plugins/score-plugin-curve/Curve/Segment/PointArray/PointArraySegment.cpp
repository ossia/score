// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PointArraySegment.hpp"

#include "psimpl.h"

#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/CurveSegmentModelSerialization.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <score/model/Identifier.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/ssize.hpp>
#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <ossia/math/safe_math.hpp>

#include <QIODevice>

#include <wobjectimpl.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>

SCORE_SERALIZE_DATASTREAM_DEFINE(Curve::PointArraySegmentData)
W_OBJECT_IMPL(Curve::PointArraySegment)
namespace Curve
{
namespace
{
const std::shared_ptr<const PointArraySamples>& noSamples()
{
  static const auto empty = std::make_shared<const PointArraySamples>();
  return empty;
}

double safeSpan(double lo, double hi) noexcept
{
  const double d = hi - lo;
  return std::abs(d) < 1e-12 ? 1e-12 : d;
}

//! y of the samples at x, interpolated linearly, clamped to the first and
//! last ones.
double interpolate(const PointArraySamples& pts, double x) noexcept
{
  if(pts.empty())
    return 0.;
  auto it = pts.upper_bound(x);
  if(it == pts.begin())
    return it->second;
  if(it == pts.end())
    return std::prev(it)->second;
  auto prev = std::prev(it);
  const double t = (x - prev->first) / (it->first - prev->first);
  return prev->second + t * (it->second - prev->second);
}

//! What the executor calls, on the audio thread: immutable, shared with the
//! model's data, no allocation.
struct PointArrayEvaluator
{
  std::shared_ptr<const PointArraySamples> points;
  double min_x{}, span_x{1.};
  double min_y{}, span_y{1.};
  double offset{}, factor{1.};

  double operator()(double ratio) const noexcept
  {
    const double v = interpolate(*points, min_x + ratio * span_x);
    return offset + factor * (v - min_y) / span_y;
  }
};

template <typename Y>
ossia::curve_segment<Y> makeFunction(std::shared_ptr<const PointArrayEvaluator> e)
{
  if constexpr(std::is_integral_v<Y>)
    return [e](double ratio, Y, Y) -> Y { return Y(std::lround((*e)(ratio))); };
  else
    return [e](double ratio, Y, Y) -> Y { return Y((*e)(ratio)); };
}
}

bool PointArraySegmentData::operator==(const PointArraySegmentData& other) const noexcept
{
  if(min_x != other.min_x || max_x != other.max_x || min_y != other.min_y
     || max_y != other.max_y)
    return false;
  if(points == other.points)
    return true;
  const auto& a = points ? *points : *noSamples();
  const auto& b = other.points ? *other.points : *noSamples();
  return a == b;
}

PointArraySegment::PointArraySegment(const Id<SegmentModel>& id, QObject* parent)
    : SegmentModel{id, parent}
    , m_points{noSamples()}
{
}

PointArraySegment::PointArraySegment(const SegmentData& dat, QObject* parent)
    : SegmentModel{dat, parent}
    , m_points{noSamples()}
{
  setData(dat.specificSegmentData.value<PointArraySegmentData>());
}

PointArraySegment::PointArraySegment(DataStream::Deserializer& vis, QObject* parent)
    : SegmentModel{vis, parent}
    , m_points{noSamples()}
{
  vis.writeTo(*this);
}

PointArraySegment::PointArraySegment(JSONObject::Deserializer& vis, QObject* parent)
    : SegmentModel{vis, parent}
    , m_points{noSamples()}
{
  vis.writeTo(*this);
}

PointArraySegment::PointArraySegment(
    const PointArraySegment& other, const id_type& id, QObject* parent)
    : SegmentModel{other.start(), other.end(), id, parent}
    , min_x{other.min_x}
    , max_x{other.max_x}
    , min_y{other.min_y}
    , max_y{other.max_y}
    , m_points{other.m_points}
{
}

PointArraySegment::~PointArraySegment() = default;

void PointArraySegment::setData(const PointArraySegmentData& dat)
{
  min_x = dat.min_x;
  max_x = dat.max_x;
  min_y = dat.min_y;
  max_y = dat.max_y;
  m_points = dat.points ? dat.points : noSamples();

  m_valid = false;
  dataChanged();
}

void PointArraySegment::setSpecificData(const QVariant& data)
{
  auto dat = data.value<PointArraySegmentData>();
  if(dat.min_x == min_x && dat.max_x == max_x && dat.min_y == min_y
     && dat.max_y == max_y && dat.points == m_points)
    return;
  setData(dat);
}

bool PointArraySegment::specificDataEquals(const QVariant& data) const noexcept
{
  return data.value<PointArraySegmentData>()
         == PointArraySegmentData{min_x, max_x, min_y, max_y, m_points};
}

QVariant PointArraySegment::toSegmentSpecificData() const
{
  return QVariant::fromValue(PointArraySegmentData{min_x, max_x, min_y, max_y, m_points});
}

PointArraySamples& PointArraySegment::editPoints()
{
  // Samples are always allocated non-const (the shared empty set is never
  // unique here): writing through m_points is fine while it is not shared.
  if(m_points.use_count() != 1)
    m_points = std::make_shared<PointArraySamples>(*m_points);
  m_valid = false;
  m_pyramidOf.reset();
  return const_cast<PointArraySamples&>(*m_points);
}

const MinMaxPyramid& PointArraySegment::pyramid() const
{
  if(m_pyramidOf.owner_before(m_points) || m_points.owner_before(m_pyramidOf)
     || m_pyramidOf.expired())
  {
    const auto& pts = *m_points;
    m_pyramid.build(pts.size(), [&](std::size_t i) { return pts.begin()[i].second; });
    m_pyramidOf = m_points;
  }
  return m_pyramid;
}

void PointArraySegment::on_startChanged()
{
  dataChanged();
}

void PointArraySegment::on_endChanged()
{
  dataChanged();
}

std::vector<QPointF> PointArraySegment::mappedPoints() const
{
  std::vector<QPointF> res;
  const auto& pts = *m_points;
  const double sx = safeSpan(min_x, max_x);
  const double sy = safeSpan(min_y, max_y);
  const double w = m_end.x() - m_start.x();

  auto first = pts.lower_bound(std::min(min_x, max_x));
  auto last = pts.upper_bound(std::max(min_x, max_x));
  res.reserve(std::distance(first, last));
  for(auto it = first; it != last; ++it)
    res.push_back(
        {m_start.x() + w * (it->first - min_x) / sx, (it->second - min_y) / sy});
  return res;
}

void PointArraySegment::updateData(int numInterp) const
{
  const int columns = std::max(numInterp, 2);
  if(m_valid && m_dataResolution == columns)
    return;
  m_valid = true;
  m_dataResolution = columns;

  static std::vector<QPointF> buffer;
  envelope(m_start.x(), m_end.x(), columns, buffer);
  m_data.assign(buffer.begin(), buffer.end());
}

void PointArraySegment::envelope(
    double x0, double x1, int columns, std::vector<QPointF>& out) const
{
  out.clear();
  const auto& pts = *m_points;
  if(pts.empty())
    return;

  // Curve abscissas to the samples' own, and back.
  const double w = m_end.x() - m_start.x();
  const double sx = safeSpan(min_x, max_x);
  const double sy = safeSpan(min_y, max_y);
  if(std::abs(w) < 1e-12)
    return;
  auto to_sample = [&](double x) { return min_x + (x - m_start.x()) / w * sx; };

  // Only within the segment: it may have been cropped.
  const double lo = std::min(min_x, max_x);
  const double hi = std::max(min_x, max_x);
  const double u0 = std::clamp(to_sample(std::max(x0, m_start.x())), lo, hi);
  const double u1 = std::clamp(to_sample(std::min(x1, m_end.x())), lo, hi);
  if(!(u1 > u0))
    return;

  const std::size_t first = pts.lower_bound(lo) - pts.begin();
  const std::size_t last = pts.upper_bound(hi) - pts.begin();
  const auto base = pts.begin() + first;
  // The columns asked for span [x0, x1]; only [u0, u1] of it has samples.
  const double requested = std::abs(to_sample(x1) - to_sample(x0));
  const auto columnsIn = std::max(
      1, int(std::lround(columns * (u1 - u0) / std::max(requested, 1e-300))));

  const bool from_start = u0 <= lo;
  if(from_start)
    out.emplace_back(lo, interpolate(pts, lo));
  static std::vector<QPointF> inner;
  Curve::envelope(
      last - first, [&](std::size_t i) { return base[i].first; },
      [&](std::size_t i) { return base[i].second; }, pyramid(), first, u0, u1,
      columnsIn, inner);
  out.insert(out.end(), inner.begin(), inner.end());
  if(u1 >= hi)
    out.emplace_back(hi, interpolate(pts, hi));

  for(auto& p : out)
    p = {m_start.x() + w * (p.x() - min_x) / sx, (p.y() - min_y) / sy};
}

std::size_t PointArraySegment::samplesBetween(double x0, double x1) const noexcept
{
  const double w = m_end.x() - m_start.x();
  if(std::abs(w) < 1e-12)
    return 0;
  const double sx = max_x - min_x;
  double u0 = min_x + (std::max(x0, m_start.x()) - m_start.x()) / w * sx;
  double u1 = min_x + (std::min(x1, m_end.x()) - m_start.x()) / w * sx;
  if(u0 > u1)
    std::swap(u0, u1);
  const auto& pts = *m_points;
  return std::distance(pts.lower_bound(u0), pts.upper_bound(u1));
}

void PointArraySegment::envelopeColumns(
    double x0, double width, int columns, std::vector<QLineF>& out) const
{
  out.clear();
  const auto& pts = *m_points;
  const double w = m_end.x() - m_start.x();
  if(pts.empty() || std::abs(w) < 1e-12)
    return;

  // In the samples' own units, within the segment.
  const double sx = safeSpan(min_x, max_x);
  const double sy = safeSpan(min_y, max_y);
  const double lo = std::min(min_x, max_x);
  const double hi = std::max(min_x, max_x);
  const std::size_t first = pts.lower_bound(lo) - pts.begin();
  const std::size_t last = pts.upper_bound(hi) - pts.begin();
  const auto base = pts.begin() + first;
  const double k = sx / w;

  Curve::envelopeColumns(
      last - first, [&](std::size_t i) { return base[i].first; },
      [&](std::size_t i) { return base[i].second; }, pyramid(), first,
      min_x + (x0 - m_start.x()) * k, width * k, columns, out);

  for(auto& l : out)
    l = {m_start.x() + w * (l.x1() - min_x) / sx, (l.y1() - min_y) / sy,
         m_start.x() + w * (l.x2() - min_x) / sx, (l.y2() - min_y) / sy};
}

double PointArraySegment::valueAt(double x) const
{
  const double w = m_end.x() - m_start.x();
  const double ratio = std::abs(w) < 1e-12 ? 1. : (x - m_start.x()) / w;
  const double v = interpolate(*m_points, min_x + ratio * (max_x - min_x));
  return (v - min_y) / safeSpan(min_y, max_y);
}

void PointArraySegment::addPoint(double x, double y)
{
  // If x < start.x() or x > end.x(), we update start / end
  // The points must keep their apparent position.
  // If y < 0 or y > 1, we rescale everything (and update min / max)
  const bool first = m_points->empty();

  if(!first)
  {
    if(x < min_x)
      min_x = x;
    else if(x > max_x)
      max_x = x;

    if(y < min_y)
      min_y = y;
    else if(y > max_y)
      max_y = y;
  }
  else
  {
    min_x = x;
    max_x = x;
    min_y = y;
    max_y = y;
  }

  editPoints()[x] = y;

  m_valid = false;
  dataChanged();
}

void PointArraySegment::addPointUnscaled(double x, double y)
{
  auto& points = editPoints();
  points[x] = y;
  const auto end = points.end();
  if(m_lastX != -1)
  {
    if(m_lastX < x)
    {
      auto it1 = points.find(m_lastX);
      auto it2 = points.lower_bound(x);
      if(it1 != end && it2 != end)
      {
        std::advance(it1, 1);
        if(it1 != end && it1 != it2)
        {
          points.erase(it1, it2);
        }
      }
    }
    else if(x < m_lastX && points.size() > 1)
    {
      auto it1 = points.find(x);
      auto it2 = points.lower_bound(m_lastX);

      if(it1 != end && it2 != end)
      {
        std::advance(it1, 1);
        if(it1 != end && it1 != it2)
        {
          points.erase(it1, it2);
        }
      }
    }
  }
  m_lastX = x;

  m_valid = false;
  dataChanged();
}

void PointArraySegment::simplify(double ratio)
{
  double tolerance = (max_y - min_y) / ratio;

  ossia::double_vector orig;
  orig.reserve(m_points->size() * 2);
  for(const auto& pt : *m_points)
  {
    orig.push_back(pt.first);
    orig.push_back(pt.second);
  }

  ossia::double_vector result;
  result.reserve(m_points->size() / 2);

  psimpl::simplify_reumann_witkam<2>(
      orig.begin(), orig.end(), tolerance, std::back_inserter(result));
  SCORE_ASSERT(result.size() % 2 == 0);

  auto& points = editPoints();
  points.clear();
  for(auto i = 0u; i < result.size(); i += 2)
    points.insert(std::make_pair(result[i], result[i + 1]));
}

std::vector<SegmentData> PointArraySegment::toLinearSegments() const
{
  std::vector<SegmentData> vec;
  const auto pts = mappedPoints();
  if(pts.size() < 2)
    return vec;
  vec.reserve(pts.size() - 1);

  int N0 = 10000;
  vec.emplace_back(
      Id<SegmentModel>{N0}, pts[0], pts[1], std::nullopt, std::nullopt,
      Metadata<ConcreteKey_k, LinearSegment>::get(),
      QVariant::fromValue(LinearSegmentData{}));

  int size = std::ssize(pts);
  for(int i = 1; i < size - 1; i++)
  {
    const int k = i + N0;
    vec.back().following = Id<SegmentModel>{k};

    vec.emplace_back(
        Id<SegmentModel>{k}, pts[i], pts[i + 1], Id<SegmentModel>{k - 1}, std::nullopt,
        Metadata<ConcreteKey_k, LinearSegment>::get(),
        QVariant::fromValue(LinearSegmentData()));
  }

  return vec;
}

std::vector<SegmentData> PointArraySegment::toPowerSegments() const
{
  std::vector<SegmentData> vec;
  const auto pts = mappedPoints();
  if(pts.size() < 2)
    return vec;
  vec.reserve(pts.size() - 1);

  int N0 = 10000;
  vec.emplace_back(
      Id<SegmentModel>{N0}, pts[0], pts[1], std::nullopt, std::nullopt,
      Metadata<ConcreteKey_k, PowerSegment>::get(),
      QVariant::fromValue(PowerSegmentData{}));

  int size = std::ssize(pts);
  for(int i = 1; i < size - 1; i++)
  {
    const int k = i + N0;
    vec.back().following = Id<SegmentModel>{k};

    vec.emplace_back(
        Id<SegmentModel>{k}, pts[i], pts[i + 1], Id<SegmentModel>{k - 1},
        OptionalId<SegmentModel>{}, Metadata<ConcreteKey_k, PowerSegment>::get(),
        QVariant::fromValue(PowerSegmentData()));
  }

  return vec;
}

void PointArraySegment::reserve(std::size_t p)
{
  editPoints().reserve(p);
}

ossia::curve_segment<double> PointArraySegment::makeDoubleFunction() const
{
  return makeScaledDoubleFunction(0., 1.);
}

ossia::curve_segment<float> PointArraySegment::makeFloatFunction() const
{
  return makeScaledFloatFunction(0., 1.);
}

ossia::curve_segment<int> PointArraySegment::makeIntFunction() const
{
  return makeScaledIntFunction(0., 1.);
}

namespace
{
std::shared_ptr<const PointArrayEvaluator> evaluator(
    std::shared_ptr<const PointArraySamples> pts, double min_x, double max_x,
    double min_y, double max_y, double offset, double factor)
{
  return std::make_shared<const PointArrayEvaluator>(PointArrayEvaluator{
      std::move(pts), min_x, max_x - min_x, min_y, safeSpan(min_y, max_y), offset,
      factor});
}
}

ossia::curve_segment<double>
PointArraySegment::makeScaledDoubleFunction(double offset, double factor) const
{
  return makeFunction<double>(
      evaluator(m_points, min_x, max_x, min_y, max_y, offset, factor));
}

ossia::curve_segment<float>
PointArraySegment::makeScaledFloatFunction(double offset, double factor) const
{
  return makeFunction<float>(
      evaluator(m_points, min_x, max_x, min_y, max_y, offset, factor));
}

ossia::curve_segment<int>
PointArraySegment::makeScaledIntFunction(double offset, double factor) const
{
  return makeFunction<int>(
      evaluator(m_points, min_x, max_x, min_y, max_y, offset, factor));
}

void PointArraySegment::reset()
{
  min_x = 0;
  max_x = 0;
  min_y = 0;
  max_y = 0;
  m_lastX = -1;
  m_points = noSamples();
  m_valid = false;
  dataChanged();
}

void setSegmentExtent(SegmentData& seg, QPointF start, QPointF end)
{
  if(seg.type == Metadata<ConcreteKey_k, PointArraySegment>::get())
  {
    // The samples at the new ends are where the old extent put them.
    auto dat = seg.specificSegmentData.value<PointArraySegmentData>();
    const double w = seg.end.x() - seg.start.x();
    if(std::abs(w) > 1e-12)
    {
      const double k = (dat.max_x - dat.min_x) / w;
      const double min_x = dat.min_x + (start.x() - seg.start.x()) * k;
      const double max_x = dat.min_x + (end.x() - seg.start.x()) * k;
      dat.min_x = min_x;
      dat.max_x = max_x;
      seg.specificSegmentData = QVariant::fromValue(std::move(dat));
    }
  }
  seg.start = start;
  seg.end = end;
}

std::vector<SegmentData>
curveFromSamples(std::span<const float> values, std::size_t editable_max)
{
  std::vector<SegmentData> segs;
  const std::size_t n = values.size();

  // Non-finite values are gaps: the others keep their place.
  std::vector<std::pair<double, double>> pts;
  pts.reserve(n);
  const double last = n > 1 ? double(n - 1) : 1.;
  for(std::size_t i = 0; i < n; i++)
    if(ossia::safe_isfinite(values[i]))
      pts.emplace_back(double(i), double(values[i]));
  if(pts.empty())
    return segs;

  if(pts.size() == 1)
  {
    const double v = pts.front().second;
    segs.emplace_back(
        Id<SegmentModel>{0}, QPointF{0., v}, QPointF{1., v}, std::nullopt, std::nullopt,
        Metadata<ConcreteKey_k, LinearSegment>::get(),
        QVariant::fromValue(LinearSegmentData{}));
    return segs;
  }

  const std::size_t m = pts.size();
  if(m <= editable_max)
  {
    segs.reserve(m - 1);
    for(std::size_t i = 0; i + 1 < m; i++)
    {
      const int id = int(i);
      segs.emplace_back(
          Id<SegmentModel>{id}, QPointF{pts[i].first / last, pts[i].second},
          QPointF{pts[i + 1].first / last, pts[i + 1].second},
          i > 0 ? OptionalId<SegmentModel>{Id<SegmentModel>{id - 1}} : std::nullopt,
          i + 2 < m ? OptionalId<SegmentModel>{Id<SegmentModel>{id + 1}} : std::nullopt,
          Metadata<ConcreteKey_k, LinearSegment>::get(),
          QVariant::fromValue(LinearSegmentData{}));
    }
    return segs;
  }

  // Sample i at x = i: already sorted.
  const QPointF start{pts.front().first / last, pts.front().second};
  const QPointF end{pts.back().first / last, pts.back().second};
  const double min_x = pts.front().first, max_x = pts.back().first;
  auto samples = std::make_shared<PointArraySamples>(
      boost::container::ordered_unique_range, pts.begin(), pts.end());

  segs.emplace_back(
      Id<SegmentModel>{0}, start, end, std::nullopt, std::nullopt,
      Metadata<ConcreteKey_k, PointArraySegment>::get(),
      QVariant::fromValue(PointArraySegmentData{min_x, max_x, 0., 1., std::move(samples)}));
  return segs;
}

std::vector<SegmentData> editableSegments(
    const SegmentData& pointArray, double tolerance, SegmentIdAllocator& ids)
{
  std::vector<SegmentData> res;
  const auto dat = pointArray.specificSegmentData.value<PointArraySegmentData>();
  if(!dat.points)
    return res;

  // x in units of samples: the distance to the simplified line is then, in
  // effect, the error in y.
  const double sy = safeSpan(dat.min_y, dat.max_y);
  const auto& pts = *dat.points;
  const double lo = std::min(dat.min_x, dat.max_x);
  const double hi = std::max(dat.min_x, dat.max_x);
  auto first = pts.upper_bound(lo);
  auto last = pts.lower_bound(hi);
  const double count = double(std::distance(first, last)) + 1.;
  const double unit = count / safeSpan(lo, hi);

  ossia::double_vector coords;
  coords.reserve(2 * (count + 2));
  auto push = [&](double x, double y) {
    coords.push_back((x - lo) * unit);
    coords.push_back((y - dat.min_y) / sy);
  };
  push(lo, interpolate(pts, lo));
  for(auto it = first; it != last; ++it)
    push(it->first, it->second);
  push(hi, interpolate(pts, hi));

  ossia::double_vector simplified;
  simplified.reserve(coords.size() / 4 + 4);
  psimpl::simplify_douglas_peucker<2>(
      coords.begin(), coords.end(), tolerance, std::back_inserter(simplified));

  // Back to curve coordinates.
  const double w = pointArray.end.x() - pointArray.start.x();
  const double sx = safeSpan(dat.min_x, dat.max_x);
  auto curve_x = [&](double sample_x) {
    const double x = lo + sample_x / unit;
    return pointArray.start.x() + w * (x - dat.min_x) / sx;
  };

  const std::size_t n = simplified.size() / 2;
  if(n < 2)
    return res;
  res.reserve(n - 1);
  for(std::size_t i = 0; i + 1 < n; i++)
  {
    SegmentData d;
    d.id = ids.next();
    d.start = {curve_x(simplified[2 * i]), simplified[2 * i + 1]};
    d.end = {curve_x(simplified[2 * i + 2]), simplified[2 * i + 3]};
    d.type = Metadata<ConcreteKey_k, LinearSegment>::get();
    d.specificSegmentData = QVariant::fromValue(LinearSegmentData{});
    if(!res.empty())
    {
      d.previous = res.back().id;
      res.back().following = d.id;
    }
    res.push_back(std::move(d));
  }

  // The ends stay where the curve had them.
  res.front().start = pointArray.start;
  res.front().previous = pointArray.previous;
  res.back().end = pointArray.end;
  res.back().following = pointArray.following;
  return res;
}
}

template <>
void DataStreamReader::read(const Curve::PointArraySegment& segmt)
{
  readFrom(segmt.toSegmentSpecificData().value<Curve::PointArraySegmentData>());
}

template <>
void DataStreamWriter::write(Curve::PointArraySegment& segmt)
{
  Curve::PointArraySegmentData dat;
  writeTo(dat);
  segmt.setData(dat);
}

template <>
void JSONReader::read(const Curve::PointArraySegment& segmt)
{
  readFrom(segmt.toSegmentSpecificData().value<Curve::PointArraySegmentData>());
}

template <>
void JSONWriter::write(Curve::PointArraySegment& segmt)
{
  Curve::PointArraySegmentData dat;
  writeTo(dat);
  segmt.setData(dat);
}

namespace
{
//! Samples come back sorted: anything else is sorted, keeping the first of
//! duplicate abscissas.
std::shared_ptr<const Curve::PointArraySamples>
samplesFrom(std::vector<std::pair<double, double>>&& pts)
{
  auto by_x = [](const auto& a, const auto& b) { return a.first < b.first; };
  if(!std::is_sorted(pts.begin(), pts.end(), by_x))
    std::stable_sort(pts.begin(), pts.end(), by_x);
  pts.erase(
      std::unique(
          pts.begin(), pts.end(),
          [](const auto& a, const auto& b) { return a.first == b.first; }),
      pts.end());
  std::erase_if(pts, [](const auto& p) {
    return !ossia::safe_isfinite(p.first) || !ossia::safe_isfinite(p.second);
  });
  return std::make_shared<Curve::PointArraySamples>(
      boost::container::ordered_unique_range, pts.begin(), pts.end());
}
}

template <>
void DataStreamReader::read(const Curve::PointArraySegmentData& segmt)
{
  m_stream << segmt.min_x << segmt.max_x << segmt.min_y << segmt.max_y;
  const auto* pts = segmt.points.get();
  m_stream << int64_t(pts ? pts->size() : 0);
  if(pts)
    for(const auto& [x, y] : *pts)
      m_stream << x << y;
}

template <>
void DataStreamWriter::write(Curve::PointArraySegmentData& segmt)
{
  int64_t n{};
  m_stream >> segmt.min_x >> segmt.max_x >> segmt.min_y >> segmt.max_y >> n;
  // A corrupt count must not allocate more than the stream holds.
  if(auto dev = m_stream.stream.device())
    n = std::min<int64_t>(n, dev->bytesAvailable() / int64_t(2 * sizeof(double)));
  std::vector<std::pair<double, double>> pts;
  pts.resize(std::max<int64_t>(n, 0));
  for(auto& [x, y] : pts)
    m_stream >> x >> y;
  segmt.points = samplesFrom(std::move(pts));
}

template <>
void JSONReader::read(const Curve::PointArraySegmentData& segmt)
{
  obj["MinX"] = segmt.min_x;
  obj["MaxX"] = segmt.max_x;
  obj["MinY"] = segmt.min_y;
  obj["MaxY"] = segmt.max_y;

  // [x0, y0, x1, y1, ...]
  stream.Key("Points");
  stream.StartArray();
  if(segmt.points)
  {
    for(const auto& [x, y] : *segmt.points)
    {
      stream.Double(x);
      stream.Double(y);
    }
  }
  stream.EndArray();
}

template <>
void JSONWriter::write(Curve::PointArraySegmentData& segmt)
{
  // Missing from segments saved before these were serialized.
  auto get = [&](const char* key, double def) {
    auto it = base.FindMember(key);
    return it != base.MemberEnd() && it->value.IsNumber() ? it->value.GetDouble() : def;
  };
  segmt.min_x = get("MinX", 0.);
  segmt.max_x = get("MaxX", 1.);
  segmt.min_y = get("MinY", 0.);
  segmt.max_y = get("MaxY", 1.);

  std::vector<std::pair<double, double>> pts;
  if(auto it = base.FindMember("Points"); it != base.MemberEnd() && it->value.IsArray())
  {
    const auto& arr = it->value.GetArray();
    pts.reserve(arr.Size() / 2);
    for(rapidjson::SizeType i = 0; i + 1 < arr.Size(); i += 2)
      if(arr[i].IsNumber() && arr[i + 1].IsNumber())
        pts.emplace_back(arr[i].GetDouble(), arr[i + 1].GetDouble());
  }
  segmt.points = samplesFrom(std::move(pts));
}
