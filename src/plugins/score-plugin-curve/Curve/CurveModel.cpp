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

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/math/safe_math.hpp>
#include <ossia/network/domain/domain_base.hpp>

#include <QDebug>
#include <QSignalBlocker>

#include <algorithm>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Curve::Model)

namespace Curve
{
namespace
{
struct ChainLinks
{
  int32_t id;
  int32_t previous;
  int32_t following;
  bool has_previous;
  bool has_following;
  double x;
};

ChainLinks linksOf(const SegmentData& s) noexcept
{
  return {
      s.id.val(),
      s.previous ? s.previous->val() : 0,
      s.following ? s.following->val() : 0,
      bool(s.previous),
      bool(s.following),
      s.start.x()};
}

ChainLinks linksOf(const SegmentModel& s) noexcept
{
  const auto& p = s.previous();
  const auto& f = s.following();
  return {
      s.id().val(), p ? p->val() : 0, f ? f->val() : 0, bool(p), bool(f), s.start().x()};
}

//! GUI thread only: reused across edits.
struct ChainScratch
{
  ossia::pod_vector<ChainLinks> links;
  ossia::hash_map<int32_t, uint32_t> index;
  ossia::pod_vector<uint32_t> heads;
  ossia::pod_vector<uint32_t> order;
};

ChainScratch& chainScratch() noexcept
{
  static ChainScratch s;
  return s;
}

//! Fills s.order with the order in which the segments of s.links must be
//! seen: chain by chain, each segment right after its previous one. False
//! when the links do not describe chains: duplicate ids, links to missing
//! segments, links that are not mutual, or cycles.
bool chainOrder(ChainScratch& s)
{
  const auto& l = s.links;
  const auto n = uint32_t(l.size());
  s.index.clear();
  s.index.reserve(n);
  for(uint32_t i = 0; i < n; i++)
    if(!s.index.emplace(l[i].id, i).second)
      return false;

  auto linked = [&](int32_t other, auto&& back_link) {
    auto it = s.index.find(other);
    return it != s.index.end() && back_link(l[it->second]);
  };

  s.heads.clear();
  for(uint32_t i = 0; i < n; i++)
  {
    const auto& c = l[i];
    if(c.has_previous)
    {
      if(!linked(c.previous, [&](const ChainLinks& p) {
        return p.has_following && p.following == c.id;
      }))
        return false;
    }
    else
    {
      s.heads.push_back(i);
    }
    if(c.has_following)
    {
      if(!linked(c.following, [&](const ChainLinks& f) {
        return f.has_previous && f.previous == c.id;
      }))
        return false;
    }
  }

  std::sort(s.heads.begin(), s.heads.end(), [&](uint32_t a, uint32_t b) {
    return l[a].x < l[b].x || (l[a].x == l[b].x && a < b);
  });

  s.order.clear();
  s.order.reserve(n);
  for(auto cur : s.heads)
  {
    for(;;)
    {
      s.order.push_back(cur);
      if(s.order.size() > n || !l[cur].has_following)
        break;
      cur = s.index.find(l[cur].following)->second;
    }
  }

  // Segments in a cycle are reachable from no head.
  return s.order.size() == n;
}

struct RelinkScratch
{
  std::vector<PointModel*> points;
  std::vector<SegmentModel*> segments;
  std::vector<SegmentModel*> added;
};

RelinkScratch& relinkScratch() noexcept
{
  static RelinkScratch s;
  return s;
}

//! Whether b directly follows a in its chain.
bool joined(const SegmentModel& a, const SegmentModel& b) noexcept
{
  return a.following() == b.id() && b.previous() == a.id();
}

struct ChangeScratch
{
  ossia::hash_set<int32_t> ids;
  std::vector<Id<SegmentModel>> removed;
  std::vector<const SegmentData*> upserted;
  std::vector<SegmentModel*> created;
};

ChangeScratch& changeScratch() noexcept
{
  static ChangeScratch s;
  return s;
}
}

bool isValidCurve(std::span<const SegmentData> curve) noexcept
{
  auto& s = chainScratch();
  s.links.clear();
  s.links.reserve(curve.size());
  for(const auto& d : curve)
  {
    if(!ossia::safe_isfinite(d.start.x()) || !ossia::safe_isfinite(d.start.y())
       || !ossia::safe_isfinite(d.end.x()) || !ossia::safe_isfinite(d.end.y()))
      return false;
    if(d.start.x() > d.end.x())
      return false;
    s.links.push_back(linksOf(d));
  }
  return chainOrder(s);
}

Model::Model(const Id<Model>& id, QObject* parent)
    : IdentifiedObject<Model>(id, QStringLiteral("CurveModel"), parent)
{
}

// Deleting a child makes Qt search it in the parent's list of children: one
// at a time, that is quadratic. QObject deletes its own children without the
// search, so the segments are left to it rather than to m_segments.
Model::~Model()
{
  m_segments.m_map.clear();
}

void Model::unlinkPoints(SegmentModel& m) noexcept
{
  m.m_startPoint = nullptr;
  m.m_endPoint = nullptr;
}

void Model::relink()
{
  rebuildOrder();
  relinkPoints();
}

void Model::relinkAfterChanges(std::span<SegmentModel* const> added)
{
  if(!spliceOrder(added))
    rebuildOrder();
  relinkPoints();
}

// m_sorted, without the segments removed since (their points were unlinked),
// merged with the added ones by x; then checked to be in chain order.
bool Model::spliceOrder(std::span<SegmentModel* const> added)
{
  auto& r = relinkScratch();
  r.added.assign(added.begin(), added.end());
  auto by_x = [](const SegmentModel* a, const SegmentModel* b) {
    return a->start().x() < b->start().x();
  };
  std::stable_sort(r.added.begin(), r.added.end(), by_x);

  auto& out = r.segments;
  out.clear();
  out.reserve(m_sorted.size() + r.added.size());
  auto next_added = r.added.begin();
  for(auto seg : m_sorted)
  {
    if(!seg->m_endPoint)
      continue;
    while(next_added != r.added.end() && by_x(*next_added, seg))
      out.push_back(*next_added++);
    out.push_back(seg);
  }
  out.insert(out.end(), next_added, r.added.end());

  if(out.size() != m_segments.size())
    return false;
  for(std::size_t i = 1; i < out.size(); i++)
  {
    const auto& a = *out[i - 1];
    const auto& b = *out[i];
    if(b.start().x() < a.start().x())
      return false;
    if(a.following() ? !joined(a, b) : bool(b.previous()))
      return false;
  }
  if(!out.empty() && (out.front()->previous() || out.back()->following()))
    return false;

  std::swap(m_sorted, out);
  out.clear();
  return true;
}

bool Model::rebuildOrder()
{
  auto& r = relinkScratch();
  auto& c = chainScratch();
  r.segments.clear();
  c.links.clear();
  for(auto& seg : m_segments)
  {
    r.segments.push_back(&seg);
    c.links.push_back(linksOf(seg));
  }

  m_sorted.clear();
  const bool ok = chainOrder(c);
  if(ok)
  {
    for(auto i : c.order)
      m_sorted.push_back(r.segments[i]);
  }
  else
  {
    // Links that do not form chains: the points follow the links that do.
    m_sorted.assign(r.segments.begin(), r.segments.end());
    std::sort(m_sorted.begin(), m_sorted.end(), [](SegmentModel* a, SegmentModel* b) {
      return a->start().x() < b->start().x()
             || (a->start().x() == b->start().x() && a->id() < b->id());
    });
  }
  r.segments.clear();
  return ok;
}

// Each segment keeps the point objects it had, when no other one took them
// first: no lookup, and selections and views stay on the same objects.
void Model::relinkPoints()
{
  auto& r = relinkScratch();
  const uint32_t pass = ++m_relinkPass;

  auto claim = [pass](PointModel* pt) -> PointModel* {
    if(pt && pt->m_relinkPass != pass)
    {
      pt->m_relinkPass = pass;
      return pt;
    }
    return nullptr;
  };
  auto place = [&](PointModel* pt, const OptionalId<SegmentModel>& prev,
                   const OptionalId<SegmentModel>& foll, Point pos) {
    if(!pt)
    {
      pt = new PointModel{Id<PointModel>{m_nextPointId++}, this};
      pt->m_relinkPass = pass;
    }
    pt->setPrevious(prev);
    pt->setFollowing(foll);
    pt->setPos(pos);
    r.points.push_back(pt);
    return pt;
  };

  r.points.clear();
  r.points.reserve(m_sorted.size() + 1);
  const std::size_t n = m_sorted.size();
  for(std::size_t i = 0; i < n; i++)
  {
    auto& seg = *m_sorted[i];
    const bool joined_before = i > 0 && joined(*m_sorted[i - 1], seg);
    const bool joined_after = i + 1 < n && joined(seg, *m_sorted[i + 1]);

    PointModel* start = joined_before
                            ? r.points.back()
                            : place(claim(seg.m_startPoint), std::nullopt, seg.id(),
                                    seg.start());
    PointModel* end = place(
        claim(seg.m_endPoint), seg.id(),
        joined_after ? seg.following() : OptionalId<SegmentModel>{}, seg.end());
    seg.m_startPoint = start;
    seg.m_endPoint = end;
  }

  // Views may still show the others until curveReset.
  for(auto pt : m_points)
    if(pt->m_relinkPass != pass)
      pt->deleteLater();

  std::swap(m_points, r.points);
  r.points.clear();
}

void Model::insertSegment(SegmentModel* m)
{
  m->setParent(this);
  m_segments.insert(m);
  relink();

  segmentAdded(m);
  curveReset();
}

void Model::addSegment(SegmentModel* m)
{
  insertSegment(m);
}

void Model::removeSegment(SegmentModel* m)
{
  const auto id = m->id();
  unlinkPoints(*m);
  m_segments.remove(id);
  relink();

  segmentRemoved(id);
  delete m;
  curveReset();
}

void Model::loadSegments(const std::vector<SegmentModel*>& models)
{
  SCORE_ASSERT(m_segments.empty());
  SCORE_ASSERT(m_points.empty());

  {
    QSignalBlocker _{this};
    for(auto seg : models)
    {
      seg->setParent(this);
      m_segments.insert(seg);
    }

    auto& c = chainScratch();
    c.links.clear();
    for(auto seg : models)
      c.links.push_back(linksOf(*seg));
    if(!chainOrder(c))
    {
      // A saved curve whose links are broken: chain what is there by x.
      qWarning() << "Curve::Model: relinking a curve with inconsistent links";
      auto sorted = models;
      std::sort(sorted.begin(), sorted.end(), [](auto a, auto b) {
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
    }
    relink();
  }

  curveReset();
  changed();
}

const std::vector<SegmentModel*>& Model::sortedSegments() const noexcept
{
  return m_sorted;
}

std::vector<SegmentData> Model::toCurveData() const
{
  std::vector<SegmentData> dat;
  toCurveData(dat);
  return dat;
}

void Model::toCurveData(std::vector<SegmentData>& out) const
{
  out.resize(m_sorted.size());
  for(std::size_t i = 0; i < m_sorted.size(); i++)
    out[i] = m_sorted[i]->toSegmentData();
}

void Model::applyChanges(
    std::span<const Id<SegmentModel>> removed,
    std::span<const SegmentData* const> upserted)
{
  bool structure = false;
  const SegmentList* csl{};
  auto& created = changeScratch().created;
  created.clear();

  {
    QSignalBlocker _{this};
    auto remove = [&](SegmentModel& seg) {
      unlinkPoints(seg);
      m_segments.remove(seg.id());
      // Views may still show it until curveReset.
      seg.deleteLater();
      structure = true;
    };

    for(const auto& id : removed)
    {
      if(auto it = m_segments.find(id); it != m_segments.end())
        remove(*it);
    }

    for(const SegmentData* d : upserted)
    {
      auto it = m_segments.find(d->id);
      if(it != m_segments.end())
      {
        SegmentModel& seg = *it;
        if(seg.concreteKey() == d->type)
        {
          if(seg.previous() != d->previous || seg.following() != d->following)
          {
            seg.setPrevious(d->previous);
            seg.setFollowing(d->following);
            structure = true;
          }
          if(seg.start() != d->start || seg.end() != d->end
             || !seg.specificDataEquals(d->specificSegmentData))
          {
            // Each setter would redraw the segment: once is enough.
            {
              QSignalBlocker block{seg};
              seg.setStart(d->start);
              seg.setEnd(d->end);
              seg.setSpecificData(d->specificSegmentData);
            }
            seg.dataChanged();
          }
          continue;
        }
        remove(seg);
      }

      if(!csl)
        csl = &score::IDocument::documentContext(*this).app.interfaces<SegmentList>();
      if(auto seg = createCurveSegment(*csl, *d, this))
      {
        m_segments.insert(seg);
        created.push_back(seg);
        structure = true;
      }
      else
      {
        qWarning() << "Curve::Model: unknown segment type";
      }
    }

    if(structure)
      relinkAfterChanges(created);
  }

  if(structure)
    curveReset();
  changed();
}

void Model::fromCurveData(const std::vector<SegmentData>& curve)
{
  // Checked before anything changes: a curve that cannot be represented
  // leaves the current one untouched.
  if(!isValidCurve(curve))
  {
    qWarning() << "Curve::Model: refusing an inconsistent curve";
    return;
  }

  const SegmentList* csl{};
  auto& c = changeScratch();
  c.ids.clear();
  c.ids.reserve(curve.size());
  c.upserted.clear();
  c.upserted.reserve(curve.size());
  for(const auto& d : curve)
  {
    auto it = m_segments.find(d.id);
    if(it == m_segments.end() || it->concreteKey() != d.type)
    {
      if(!csl)
        csl = &score::IDocument::documentContext(*this).app.interfaces<SegmentList>();
      if(!csl->get(d.type))
      {
        qWarning() << "Curve::Model: refusing a curve with an unknown segment type";
        return;
      }
    }
    c.ids.insert(d.id.val());
    c.upserted.push_back(&d);
  }

  c.removed.clear();
  for(const auto& seg : m_segments)
    if(!c.ids.contains(seg.id().val()))
      c.removed.push_back(seg.id());

  applyChanges(c.removed, c.upserted);
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
  ossia::hash_set<const IdentifiedObjectAbstract*> selected;
  selected.reserve(s.size());
  for(const auto& elt : s)
    selected.insert(elt.data());

  for(auto& elt : m_segments)
    elt.selection.set(selected.contains(&elt));
  for(auto& elt : m_points)
    elt->selection.set(selected.contains(elt));
}

void Model::clear()
{
  cleared();

  for(auto& seg : m_segments)
    unlinkPoints(seg);
  m_segments.clear();
  m_points.clear();
  m_sorted.clear();

  // In the order of the list of children: each one is then first in it when
  // it goes, which Qt removes in constant time.
  for(QObject* child : children())
    if(qobject_cast<SegmentModel*>(child) || qobject_cast<PointModel*>(child))
      child->deleteLater();
}

const std::vector<PointModel*>& Model::points() const
{
  return m_points;
}

double Model::lastPointPos() const
{
  // Chains do not overlap: the last one in x order ends furthest.
  return m_sorted.empty() ? 0. : std::max(0., m_sorted.back()->end().x());
}

std::optional<double> Model::valueAt(double x) const noexcept
{
  // The last segment starting at or before x.
  auto it = std::upper_bound(
      m_sorted.begin(), m_sorted.end(), x,
      [](double x, const SegmentModel* seg) { return x < seg->start().x(); });
  if(it == m_sorted.begin())
    return {};
  const SegmentModel& seg = **std::prev(it);
  if(x <= seg.end().x())
    return seg.valueAt(x);
  return {};
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
