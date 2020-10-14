// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveModel.hpp"

#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/CurveSegmentModelSerialization.hpp>
#include <State/ValueConversion.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/selection/Selectable.hpp>
#include <score/selection/Selection.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/MapCopy.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/domain/domain_base.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Curve::Model)

namespace Curve
{
Model::Model(const Id<Model>& id, QObject* parent)
    : IdentifiedObject<Model>(id, QStringLiteral("CurveModel"), parent)
{
}

void Model::addSortedSegment(SegmentModel* m)
{
  insertSegment(m);

  // Add points if necessary
  // If there is an existing previous segment, its end point also exists
  auto createStartPoint = [&]() {
    auto pt = new PointModel{getStrongId(m_points), this};
    pt->setFollowing(m->id());
    pt->setPos(m->start());
    addPoint(pt);
    return pt;
  };
  auto createEndPoint = [&]() {
    auto pt = new PointModel{getStrongId(m_points), this};
    pt->setPrevious(m->id());
    pt->setPos(m->end());
    addPoint(pt);
    return pt;
  };

  if (!m->previous())
  {
    createStartPoint();
  }
  else
  {
    // The previous segment has already been inserted,
    // hence the previous point is present.
    SCORE_ASSERT(!m_points.empty());
    m_points.back()->setFollowing(m->id());
  }

  createEndPoint();
}

void Model::addSegment(SegmentModel* m)
{
  insertSegment(m);

  // Add points if necessary
  // If there is an existing previous segment, its end point also exists
  auto createStartPoint = [&]() {
    auto pt = new PointModel{getStrongId(m_points), this};
    pt->setFollowing(m->id());
    pt->setPos(m->start());
    addPoint(pt);
    return pt;
  };
  auto createEndPoint = [&]() {
    auto pt = new PointModel{getStrongId(m_points), this};
    pt->setPrevious(m->id());
    pt->setPos(m->end());
    addPoint(pt);
    return pt;
  };

  if (m->previous())
  {
    auto previousSegment
        = std::find_if(m_segments.begin(), m_segments.end(), [&](const auto& seg) {
            return seg.following() == m->id();
          });
    if (previousSegment != m_segments.end())
    {
      auto thePt = std::find_if(m_points.begin(), m_points.end(), [&](PointModel* pt) {
        return pt->previous() == (*previousSegment).id();
      });

      if (thePt != m_points.end())
      {
        // The previous segments and points both exist
        (*thePt)->setFollowing(m->id());
      }
      else
      {
        // The previous segment exists but not the end point.
        auto pt = createStartPoint();
        pt->setPrevious((*previousSegment).id());
      }
    }
    else // The previous segment has not yet been added.
    {
      createStartPoint();
    }
  }
  else if (std::none_of(m_points.begin(), m_points.end(), [&](PointModel* pt) {
             return pt->following() == m->id();
           }))
  {
    createStartPoint();
  }

  if (m->following())
  {
    auto followingSegment
        = std::find_if(m_segments.begin(), m_segments.end(), [&](const auto& seg) {
            return seg.previous() == m->id();
          });
    if (followingSegment != m_segments.end())
    {
      auto thePt = std::find_if(m_points.begin(), m_points.end(), [&](PointModel* pt) {
        return pt->following() == (*followingSegment).id();
      });

      if (thePt != m_points.end())
      {
        (*thePt)->setPrevious(m->id());
      }
      else
      {
        auto pt = createEndPoint();
        pt->setFollowing((*followingSegment).id());
      }
    }
    else
    {
      createEndPoint();
    }
  }
  else if (std::none_of(m_points.begin(), m_points.end(), [&](PointModel* pt) {
             return pt->previous() == m->id();
           }))
  {
    // Note : if one day a buggy case happens here, check that set
    // following/previous
    // are correctly set after cloning the segment.
    createEndPoint();
  }
}

void Model::insertSegment(SegmentModel* m)
{
  m->setParent(this);
  m_segments.insert(m);

  // TODO have indexes on the points with the start and end
  // curve segments
  connect(m, &SegmentModel::startChanged, this, [=]() {
    for (PointModel* pt : m_points)
    {
      if (pt->following() == m->id())
      {
        pt->setPos(m->start());
        break;
      }
    }
  });
  connect(m, &SegmentModel::endChanged, this, [=]() {
    for (PointModel* pt : m_points)
    {
      if (pt->previous() == m->id())
      {
        pt->setPos(m->end());
        break;
      }
    }
  });

  segmentAdded(*m);
}

void Model::removeSegment(SegmentModel* m)
{
  m_segments.remove(m->id());

  segmentRemoved(m->id());

  for (PointModel* pt : m_points)
  {
    if (pt->previous() == m->id())
    {
      pt->setPrevious(OptionalId<SegmentModel>{});
    }

    if (pt->following() == m->id())
    {
      pt->setFollowing(OptionalId<SegmentModel>{});
    }

    if (!pt->previous() && !pt->following())
    {
      removePoint(pt);
    }
  }

  delete m;
}

std::vector<SegmentModel*> Model::sortedSegments() const
{
  std::vector<SegmentModel*> dat;
  dat.reserve(m_segments.size());
  for (auto& seg : m_segments)
  {
    dat.push_back(&seg);
  }

  ossia::sort(dat, [](auto s1, auto s2) { return s1->start().x() < s2->start().x(); });

  return dat;
}

std::vector<SegmentData> Model::toCurveData() const
{
  std::vector<SegmentData> dat;
  dat.reserve(m_segments.size());
  for (const auto& seg : m_segments)
  {
    dat.push_back(seg.toSegmentData());
  }

  return dat;
}

void Model::fromCurveData(const std::vector<SegmentData>& curve)
{
  {
    QSignalBlocker _{this};
    clear();

    auto& context = score::IDocument::documentContext(*this).app;
    auto& csl = context.interfaces<SegmentList>();

    static std::vector<SegmentData> map;
    map.assign(curve.begin(), curve.end());
    std::sort(map.begin(), map.end());

    /*
    qDebug() << "Printing map: ";
    for(auto elt : map)
    {
      QString log = QStringLiteral("id: %1 [%2; %3] prev: %4 following: %5")
                  .arg(elt.id.val())
                  .arg(elt.start.x())
                  .arg(elt.end.x())
                  .arg(elt.previous.value_or(Id<Curve::SegmentModel>{-1}).val())
                  .arg(elt.following.value_or(Id<Curve::SegmentModel>{-1}).val());
      qDebug() << log;
    }
    std::cout << std::endl;
    std::cerr << std::endl;
    */
    SCORE_ASSERT(map.empty() || (!map.front().previous && !map.back().following));

    for (const auto& elt : map)
    {
      addSortedSegment(createCurveSegment(csl, elt, this));
    }
    map.clear();
  }

  curveReset();
  changed();
}

Selection Model::selectedChildren() const
{
  Selection s;
  for (const auto& elt : m_segments)
  {
    if (elt.selection.get())
      s.append(elt);
  }
  for (const auto& elt : m_points)
  {
    if (elt->selection.get())
      s.append(elt);
  }

  return s;
}

void Model::setSelection(const Selection& s)
{
  // OPTIMIZEME
  for (auto& elt : m_segments)
    elt.selection.set(s.contains(&elt));
  for (auto& elt : m_points)
    elt->selection.set(s.contains(elt));
}

void Model::clear()
{
  cleared();

  auto segs = shallow_copy(m_segments);
  m_segments.clear();
  for (auto seg : segs)
    seg->deleteLater();

  auto pts = m_points;
  m_points.clear();
  for (auto pt : pts)
    pt->deleteLater();
}

const std::vector<PointModel*>& Model::points() const
{
  return m_points;
}

double Model::lastPointPos() const
{
  double pos = 0;
  for (auto pt : m_points)
    if (pt->pos().x() > pos)
      pos = pt->pos().x();
  return pos;
}

std::optional<double> Model::valueAt(double x) const noexcept
{
  for(const Curve::SegmentModel& segment : m_segments)
  {
    if(segment.start().x() <= x && x <= segment.end().x())
    {
      return segment.valueAt(x);
    }
  }
  return {};
}

void Model::addPoint(PointModel* pt)
{
  m_points.push_back(pt);

  pointAdded(*pt);
}

void Model::removePoint(PointModel* pt)
{
  auto it = ossia::find(m_points, pt);
  if (it != m_points.end())
  {
    m_points.erase(it);
  }

  pointRemoved(pt->id());
  delete pt;
}

std::vector<SegmentData> orderedSegments(const Model& curve)
{
  auto vec = curve.toCurveData();
  std::sort(vec.begin(), vec.end());
  return vec;
}

CurveDomain::CurveDomain(const ossia::domain& dom)
    : min{ossia::convert<double>(dom.get_min())}, max{ossia::convert<double>(dom.get_max())}
{
  if (min == 0. && max == 0.)
  {
    max = 1.;
  }

  start = std::min(min, max);
  end = std::max(min, max);
}

CurveDomain::CurveDomain(const ossia::domain& dom, const ossia::value& v)
    : min{ossia::convert<double>(dom.get_min())}, max{ossia::convert<double>(dom.get_max())}
{
  if (min == 0. && max == 0.)
  {
    const auto val = State::convert::value<double>(v);
    if (val > 0.)
      max = val;
    else if (val < 0.)
      min = val;
    else
      max = 1.;
  }

  if (min == max)
    max += 1.;

  start = std::min(min, max);
  end = std::max(min, max);
}

CurveDomain::CurveDomain(const ossia::domain& dom, double start, double end)
    : min{ossia::convert<double>(dom.get_min())}
    , max{ossia::convert<double>(dom.get_max())}
    , start{start}
    , end{end}
{
  auto min_v = dom.get_min();
  auto max_v = dom.get_max();
  min = (min_v.valid()) ? std::min(ossia::convert<double>(min_v), std::min(start, end))
                        : std::min(start, end);
  max = (max_v.valid()) ? std::max(ossia::convert<double>(max_v), std::max(start, end))
                        : std::max(start, end);

  ensureValid();
}

void CurveDomain::refine(const ossia::domain& dom)
{
  auto min_v = dom.get_min();
  auto max_v = dom.get_max();

  if (min_v.valid())
    min = std::min(min, ossia::convert<double>(min_v));
  else
    min = std::min(start, end);

  if (max_v.valid())
    max = std::max(max, ossia::convert<double>(max_v));
  else
    max = std::max(start, end);

  ensureValid();
}

void CurveDomain::ensureValid()
{
  if (min == 0. && max == 0.)
  {
    max = 1.;
  }
  else if (min == max)
  {
    max += 1.;
  }

  if (start == end)
  {
    start = std::min(min, max);
    end = std::max(min, max);
  }
}
}
