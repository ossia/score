// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PointArraySegment.hpp"

#include "psimpl.h"

#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <score/model/Identifier.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <wobjectimpl.h>

#include <cstddef>
#include <functional>
W_OBJECT_IMPL(Curve::PointArraySegment)
namespace Curve
{
PointArraySegment::PointArraySegment(const SegmentData& dat, QObject* parent)
    : SegmentModel{dat, parent}
{
  const auto& pa_data = dat.specificSegmentData.value<PointArraySegmentData>();
  min_x = pa_data.min_x;
  max_x = pa_data.max_x;
  min_y = pa_data.min_y;
  max_y = pa_data.max_y;

  for (auto pt : pa_data.m_points)
  {
    m_points.insert(std::make_pair(pt.x(), pt.y()));
  }
}

PointArraySegment::PointArraySegment(
    const PointArraySegment& other,
    const id_type& id,
    QObject* parent)
    : SegmentModel{other.start(), other.end(), id, parent}
    , min_x{other.min_x}
    , max_x{other.max_x}
    , min_y{other.min_y}
    , max_y{other.max_y}
    , m_points{other.m_points}
{
}

PointArraySegment::~PointArraySegment() = default;
void PointArraySegment::on_startChanged()
{
  dataChanged();
}

void PointArraySegment::on_endChanged()
{
  dataChanged();
}

void PointArraySegment::updateData(int numInterp) const
{
  if (!m_valid)
  {
    m_data.clear();
    m_data.reserve(m_points.size());

    double length = max_x - min_x;
    if(std::abs(length) < 1e-12)
      length = 1e-12;
    double amplitude = max_y - min_y;
    if(std::abs(amplitude) < 1e-12)
      amplitude = 1e-12;

    // Scale all the points between 0 / 1 in <->
    // and the local min / max in vertical

    for (const auto& elt : m_points)
    {
      m_data.push_back(
          {(m_end.x() - m_start.x()) * (elt.first - min_x) / length + m_start.x(),
           (elt.second - min_y) / amplitude});
    }
  }
}

double PointArraySegment::valueAt(double x) const
{
  SCORE_TODO;
  return 0;
}

void PointArraySegment::addPoint(double x, double y)
{
  // If x < start.x() or x > end.x(), we update start / end
  // The points must keep their apparent position.
  // If y < 0 or y > 1, we rescale everything (and update min / max)
  int s = m_points.size();

  if (y < min_y)
    min_y = y;
  if (y > max_y)
    max_y = y;

  if (s > 0)
  {
    if (x < min_x)
      min_x = x;
    else if (x > max_x)
      max_x = x;

    if (y < min_y)
      min_y = y;
    else if (y > max_y)
      max_y = y;
  }
  else
  {
    min_x = x;
    max_x = x;
    min_y = y;
    max_y = y;
  }

  m_points[x] = y;

  m_valid = false;
  dataChanged();
}

void PointArraySegment::addPointUnscaled(double x, double y)
{
  m_points[x] = y;
  const auto end = m_points.end();
  if (m_lastX != -1)
  {
    if (m_lastX < x)
    {
      auto it1 = m_points.find(m_lastX);
      auto it2 = m_points.lower_bound(x);
      if (it1 != end && it2 != end)
      {
        std::advance(it1, 1);
        if (it1 != end && it1 != it2)
        {
          m_points.erase(it1, it2);
        }
      }
    }
    else if (x < m_lastX && m_points.size() > 1)
    {
      auto it1 = m_points.find(x);
      auto it2 = m_points.lower_bound(m_lastX);

      if (it1 != end && it2 != end)
      {
        std::advance(it1, 1);
        if (it1 != end && it1 != it2)
        {
          m_points.erase(it1, it2);
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
  orig.reserve(m_points.size() * 2);
  for (const auto& pt : m_points)
  {
    orig.push_back(pt.first);
    orig.push_back(pt.second);
  }

  ossia::double_vector result;
  result.reserve(m_points.size() / 2);

  psimpl::simplify_reumann_witkam<2>(
      orig.begin(), orig.end(), tolerance, std::back_inserter(result));
  SCORE_ASSERT(result.size() > 0);
  SCORE_ASSERT(result.size() % 2 == 0);

  m_points.clear();
  // m_points.container.reserve(result.size() / 2);
  for (auto i = 0u; i < result.size(); i += 2)
  {
    m_points.insert(std::make_pair(result[i], result[i + 1]));
  }
}

std::vector<SegmentData> PointArraySegment::toLinearSegments() const
{
  std::vector<SegmentData> vec;
  m_valid = false;
  updateData(0);
  const auto& pts = data();
  vec.reserve(pts.size() - 1);

  int N0 = 10000;
  vec.emplace_back(
      Id<SegmentModel>{N0},
      pts[0],
      pts[1],
      std::nullopt,
      std::nullopt,
      Metadata<ConcreteKey_k, LinearSegment>::get(),
      QVariant::fromValue(LinearSegmentData{}));

  int size = pts.size();
  for (int i = 1; i < size - 1; i++)
  {
    const int k = i + N0;
    vec.back().following = Id<SegmentModel>{k};

    vec.emplace_back(
        Id<SegmentModel>{k},
        pts[i],
        pts[i + 1],
        Id<SegmentModel>{k - 1},
        std::nullopt,
        Metadata<ConcreteKey_k, LinearSegment>::get(),
        QVariant::fromValue(LinearSegmentData()));
  }

  return vec;
}

std::vector<SegmentData> PointArraySegment::toPowerSegments() const
{
  std::vector<SegmentData> vec;
  m_valid = false;
  updateData(0);
  const auto& pts = data();
  vec.reserve(pts.size() - 1);

  int N0 = 10000;
  vec.emplace_back(
      Id<SegmentModel>{N0},
      pts[0],
      pts[1],
      std::nullopt,
      std::nullopt,
      Metadata<ConcreteKey_k, PowerSegment>::get(),
      QVariant::fromValue(PowerSegmentData{}));

  int size = pts.size();
  for (int i = 1; i < size - 1; i++)
  {
    const int k = i + N0;
    vec.back().following = Id<SegmentModel>{k};

    vec.emplace_back(
        Id<SegmentModel>{k},
        pts[i],
        pts[i + 1],
        Id<SegmentModel>{k - 1},
        OptionalId<SegmentModel>{},
        Metadata<ConcreteKey_k, PowerSegment>::get(),
        QVariant::fromValue(PowerSegmentData()));
  }

  return vec;
}

void PointArraySegment::reset()
{
  min_x = 0;
  max_x = 0;
  min_y = 0;
  max_y = 0;
  m_lastX = -1;
  m_points.clear();
  dataChanged();
}
}
template <>
void DataStreamReader::read(const Curve::PointArraySegment& segmt)
{
  SCORE_TODO;
}

template <>
void DataStreamWriter::write(Curve::PointArraySegment& segmt)
{
  SCORE_TODO;
}

template <>
void JSONReader::read(const Curve::PointArraySegment& segmt)
{
  SCORE_TODO;
}

template <>
void JSONWriter::write(Curve::PointArraySegment& segmt)
{
  SCORE_TODO;
}

template <>
void DataStreamReader::read(const Curve::PointArraySegmentData& segmt)
{
  SCORE_TODO;
}

template <>
void DataStreamWriter::write(Curve::PointArraySegmentData& segmt)
{
  SCORE_TODO;
}

template <>
void JSONReader::read(const Curve::PointArraySegmentData& segmt)
{
  SCORE_TODO;
}

template <>
void JSONWriter::write(Curve::PointArraySegmentData& segmt)
{
  SCORE_TODO;
}
