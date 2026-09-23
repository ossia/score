// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveModel.hpp"

#include <State/ValueConversion.hpp>

#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/CurveSegmentModelSerialization.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>
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
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/network/domain/domain_base.hpp>

#include <QDebug>
#include <QSignalBlocker>

#include <wobjectimpl.h>

#include <cmath>

W_OBJECT_IMPL(Curve::Model)

namespace Curve
{
namespace
{
struct ChainLinks
{
  int32_t id{};
  std::optional<int32_t> previous, following;
  double x{};
};

template <typename T>
  requires(!std::is_pointer_v<T>)
ChainLinks linksOf(const T& seg)
{
  auto opt = [](const OptionalId<SegmentModel>& i) -> std::optional<int32_t> {
    if(i)
      return i->val();
    return std::nullopt;
  };
  return {seg.id.val(), opt(seg.previous), opt(seg.following), seg.start.x()};
}

ChainLinks linksOf(const SegmentModel* seg)
{
  auto opt = [](const OptionalId<SegmentModel>& i) -> std::optional<int32_t> {
    if(i)
      return i->val();
    return std::nullopt;
  };
  return {seg->id().val(), opt(seg->previous()), opt(seg->following()), seg->start().x()};
}

//! The order in which addSortedSegment() must see the segments: chain by
//! chain, each segment right after its previous one. Empty when the links do
//! not describe chains: duplicate ids, links to missing segments, links that
//! are not mutual, or cycles.
std::optional<std::vector<std::size_t>> chainOrder(const std::vector<ChainLinks>& l)
{
  ossia::hash_map<int32_t, std::size_t> index;
  index.reserve(l.size());
  for(std::size_t i = 0; i < l.size(); i++)
    if(!index.emplace(l[i].id, i).second)
      return std::nullopt;

  std::vector<std::size_t> heads;
  for(std::size_t i = 0; i < l.size(); i++)
  {
    const auto& s = l[i];
    if(s.previous)
    {
      auto it = index.find(*s.previous);
      if(it == index.end() || l[it->second].following != s.id)
        return std::nullopt;
    }
    else
    {
      heads.push_back(i);
    }
    if(s.following)
    {
      auto it = index.find(*s.following);
      if(it == index.end() || l[it->second].previous != s.id)
        return std::nullopt;
    }
  }

  std::stable_sort(heads.begin(), heads.end(), [&](std::size_t a, std::size_t b) {
    return l[a].x < l[b].x;
  });

  std::vector<std::size_t> order;
  order.reserve(l.size());
  for(auto h : heads)
  {
    for(std::optional<std::size_t> cur = h; cur;)
    {
      order.push_back(*cur);
      if(order.size() > l.size())
        return std::nullopt;
      const auto& f = l[*cur].following;
      cur = f ? std::optional<std::size_t>{index.at(*f)} : std::nullopt;
    }
  }

  // Segments in a cycle are reachable from no head.
  if(order.size() != l.size())
    return std::nullopt;
  return order;
}

template <typename Container>
std::optional<std::vector<std::size_t>> chainOrder(const Container& segs)
{
  std::vector<ChainLinks> l;
  l.reserve(segs.size());
  for(const auto& s : segs)
    l.push_back(linksOf(s));
  return chainOrder(l);
}
}

bool isValidCurve(const std::vector<SegmentData>& curve) noexcept
{
  for(const auto& s : curve)
  {
    if(!std::isfinite(s.start.x()) || !std::isfinite(s.start.y())
       || !std::isfinite(s.end.x()) || !std::isfinite(s.end.y()))
      return false;
    if(s.start.x() > s.end.x())
      return false;
  }
  return bool(chainOrder(curve));
}

Model::Model(const Id<Model>& id, QObject* parent)
    : IdentifiedObject<Model>(id, QStringLiteral("CurveModel"), parent)
{
}

PointModel* Model::createStartPoint(SegmentModel* m)
{
  auto pt = new PointModel{getStrongId(m_points), this};
  pt->setFollowing(m->id());
  pt->setPos(m->start());
  addPoint(pt);
  return pt;
}

PointModel* Model::createEndPoint(SegmentModel* m)
{
  auto pt = new PointModel{getStrongId(m_points), this};
  pt->setPrevious(m->id());
  pt->setPos(m->end());
  addPoint(pt);
  return pt;
}

void Model::addSortedSegment(SegmentModel* m)
{
  insertSegment(m);

  // Add points if necessary
  // If there is an existing previous segment, its end point also exists
  if(!m->previous())
  {
    createStartPoint(m);
  }
  else
  {
    // The previous segment has already been inserted,
    // hence the previous point is present.
    SCORE_ASSERT(!m_points.empty());
    m_points.back()->setFollowing(m->id());
  }

  createEndPoint(m);
}

void Model::addSegment(SegmentModel* m)
{
  insertSegment(m);

  // Add points if necessary
  // If there is an existing previous segment, its end point also exists

  if(m->previous())
  {
    auto previousSegment
        = std::find_if(m_segments.begin(), m_segments.end(), [&](const auto& seg) {
            return seg.following() == m->id();
          });
    if(previousSegment != m_segments.end())
    {
      auto thePt = std::find_if(m_points.begin(), m_points.end(), [&](PointModel* pt) {
        return pt->previous() == (*previousSegment).id();
      });

      if(thePt != m_points.end())
      {
        // The previous segments and points both exist
        (*thePt)->setFollowing(m->id());
      }
      else
      {
        // The previous segment exists but not the end point.
        auto pt = createStartPoint(m);
        pt->setPrevious((*previousSegment).id());
      }
    }
    else // The previous segment has not yet been added.
    {
      createStartPoint(m);
    }
  }
  else if(std::none_of(m_points.begin(), m_points.end(), [&](PointModel* pt) {
            return pt->following() == m->id();
          }))
  {
    createStartPoint(m);
  }

  if(m->following())
  {
    auto followingSegment
        = std::find_if(m_segments.begin(), m_segments.end(), [&](const auto& seg) {
            return seg.previous() == m->id();
          });
    if(followingSegment != m_segments.end())
    {
      auto thePt = std::find_if(m_points.begin(), m_points.end(), [&](PointModel* pt) {
        return pt->following() == (*followingSegment).id();
      });

      if(thePt != m_points.end())
      {
        (*thePt)->setPrevious(m->id());
      }
      else
      {
        auto pt = createEndPoint(m);
        pt->setFollowing((*followingSegment).id());
      }
    }
    else
    {
      createEndPoint(m);
    }
  }
  else if(std::none_of(m_points.begin(), m_points.end(), [&](PointModel* pt) {
            return pt->previous() == m->id();
          }))
  {
    // Note : if one day a buggy case happens here, check that set
    // following/previous
    // are correctly set after cloning the segment.
    createEndPoint(m);
  }
}

void Model::insertSegment(SegmentModel* m)
{
  m->setParent(this);
  m_segments.insert(m);

  // TODO have indexes on the points with the start and end
  // curve segments
  connect(m, &SegmentModel::startChanged, this, [this, m]() {
    for(PointModel* pt : m_points)
    {
      if(pt->following() == m->id())
      {
        pt->setPos(m->start());
        break;
      }
    }
  });
  connect(m, &SegmentModel::endChanged, this, [this, m]() {
    for(PointModel* pt : m_points)
    {
      if(pt->previous() == m->id())
      {
        pt->setPos(m->end());
        break;
      }
    }
  });

  segmentAdded(m);
}

void Model::loadSegments(const std::vector<SegmentModel*>& map)
{
  SCORE_ASSERT(m_segments.empty());
  SCORE_ASSERT(m_points.empty());

  auto order = chainOrder(map);
  if(!order)
  {
    // A saved curve whose links are broken: chain what is there by x.
    qWarning() << "Curve::Model: relinking a curve with inconsistent links";
    auto sorted = map;
    std::stable_sort(sorted.begin(), sorted.end(), [](auto a, auto b) {
      return a->start().x() < b->start().x();
    });
    for(std::size_t i = 0; i < sorted.size(); i++)
    {
      sorted[i]->setPrevious(
          i > 0 ? OptionalId<SegmentModel>{sorted[i - 1]->id()} : std::nullopt);
      sorted[i]->setFollowing(
          i + 1 < sorted.size() ? OptionalId<SegmentModel>{sorted[i + 1]->id()}
                                : std::nullopt);
    }
    order = chainOrder(sorted);
    SCORE_ASSERT(order);
    return loadSegments_impl(sorted, *order);
  }
  loadSegments_impl(map, *order);
}

void Model::loadSegments_impl(
    const std::vector<SegmentModel*>& map, const std::vector<std::size_t>& order)
{
  {
    QSignalBlocker _{this};
    clear();

    for(auto i : order)
    {
      addSortedSegment(map[i]);
    }
  }

  curveReset();
  changed();
}

void Model::removeSegment(SegmentModel* m)
{
  m_segments.remove(m->id());

  segmentRemoved(m->id());

  const auto points = m_points;
  for(PointModel* pt : points)
  {
    if(pt->previous() == m->id())
    {
      pt->setPrevious(OptionalId<SegmentModel>{});
    }

    if(pt->following() == m->id())
    {
      pt->setFollowing(OptionalId<SegmentModel>{});
    }

    if(!pt->previous() && !pt->following())
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
  for(auto& seg : m_segments)
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
  for(const auto& seg : m_segments)
  {
    dat.push_back(seg.toSegmentData());
  }

  return dat;
}

void Model::fromCurveData(const std::vector<SegmentData>& curve)
{
  auto& context = score::IDocument::documentContext(*this).app;
  auto& csl = context.interfaces<SegmentList>();

  // Checked before anything is cleared: a curve that cannot be represented
  // leaves the current one untouched.
  auto order = chainOrder(curve);
  const bool known_types = ossia::all_of(
      curve, [&](const SegmentData& s) { return csl.get(s.type) != nullptr; });
  if(!order || !known_types || !isValidCurve(curve))
  {
    qWarning() << "Curve::Model: refusing an inconsistent curve";
    return;
  }

  {
    QSignalBlocker _{this};
    clear();

    for(auto i : *order)
    {
      addSortedSegment(createCurveSegment(csl, curve[i], this));
    }
  }

  curveReset();
  changed();
}

Selection Model::selectedChildren() const
{
  Selection s;
  for(const auto& elt : m_segments)
  {
    if(elt.selection.get())
      s.append(elt);
  }
  for(const auto& elt : m_points)
  {
    if(elt->selection.get())
      s.append(elt);
  }

  return s;
}

void Model::setSelection(const Selection& s)
{
  // OPTIMIZEME
  for(auto& elt : m_segments)
    elt.selection.set(s.contains(&elt));
  for(auto& elt : m_points)
    elt->selection.set(s.contains(elt));
}

void Model::clear()
{
  cleared();

  auto segs = shallow_copy(m_segments);
  m_segments.clear();
  for(auto seg : segs)
    seg->deleteLater();

  auto pts = m_points;
  m_points.clear();
  for(auto pt : pts)
    pt->deleteLater();
}

const std::vector<PointModel*>& Model::points() const
{
  return m_points;
}

double Model::lastPointPos() const
{
  double pos = 0;
  for(auto pt : m_points)
    if(pt->pos().x() > pos)
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

  pointAdded(pt);
}

void Model::removePoint(PointModel* pt)
{
  auto it = ossia::find(m_points, pt);
  if(it != m_points.end())
  {
    m_points.erase(it);
  }

  pointRemoved(pt->id());
  delete pt;
}

std::vector<SegmentData> orderedSegments(const Model& curve)
{
  // Chain order: sorting by x could put a vertical step after its follower.
  return curve.toCurveData();
}

CurveDomain::CurveDomain(const ossia::domain& dom)
    : min{ossia::convert<double>(dom.get_min())}
    , max{ossia::convert<double>(dom.get_max())}
{
  if(min == 0. && max == 0.)
  {
    max = 1.;
  }

  start = std::min(min, max);
  end = std::max(min, max);
}

CurveDomain::CurveDomain(const ossia::domain& dom, const ossia::value& v)
    : min{ossia::convert<double>(dom.get_min())}
    , max{ossia::convert<double>(dom.get_max())}
{
  const auto val = State::convert::value<double>(v);
  if(min == 0. && max == 0.)
  {
    if(val > 0.)
      max = val;
    else if(val < 0.)
      min = val;
    else
      max = 1.;
  }

  if(min == max)
    max += 1.;

  start = ossia::clamp(val, min, max);
  end = start;
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

  if(min_v.valid())
    min = std::min(min, ossia::convert<double>(min_v));
  else
    min = std::min(start, end);

  if(max_v.valid())
    max = std::max(max, ossia::convert<double>(max_v));
  else
    max = std::max(start, end);

  ensureValid();
}

void CurveDomain::ensureValid()
{
  if(min == 0. && max == 0.)
  {
    max = 1.;
  }
  else if(min == max)
  {
    max += 1.;
  }

  if(start == end)
  {
    start = std::min(min, max);
    end = std::max(min, max);
  }
}
}
