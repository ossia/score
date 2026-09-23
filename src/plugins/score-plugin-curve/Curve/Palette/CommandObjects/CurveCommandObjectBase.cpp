// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveCommandObjectBase.hpp"

#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePoint.hpp>

#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/math/safe_math.hpp>

#include <QVariant>

#include <algorithm>

namespace score
{
class CommandStackFacade;
} // namespace score

namespace Curve
{
CommandObjectBase::CommandObjectBase(
    const Model& model, Presenter* pres, const score::CommandStackFacade& stack)
    : m_model{model}
    , m_presenter{pres}
    , m_dispatcher{stack}
{
}

CommandObjectBase::~CommandObjectBase() { }

void CommandObjectBase::press()
{
  // Serialize the current state of the curve
  m_model.toCurveData(m_startSegments);
  checkValidity(m_startSegments);

  // To prevent behind locked at 0.000001 or 0.9999
  m_xmin = std::numeric_limits<decltype(m_xmax)>::lowest();
  m_xmax = std::numeric_limits<decltype(m_xmax)>::max();
  m_xLastPoint = -1;

  on_press();
}

void CommandObjectBase::handleLocking()
{
  double current_x = m_state->currentPoint.x();
  double current_y = m_state->currentPoint.y();

  const bool bounded = m_presenter ? m_presenter->boundedMove() : true;
  // We lock between O - 1 in both axes.
  if(current_x < 0.)
    m_state->currentPoint.setX(0.);
  else if(bounded && current_x > 1.)
    m_state->currentPoint.setX(1.);

  if(current_y < 0.)
    m_state->currentPoint.setY(0.);
  else if(current_y > 1.)
    m_state->currentPoint.setY(1.);

  // And more specifically...
  // A bound at the original x (a vertical step) holds the point there
  // instead of pushing it past.
  if(m_presenter && m_presenter->editionSettings().lockBetweenPoints())
  {
    const double orig = m_originalPress.x();
    if(current_x <= m_xmin)
      m_state->currentPoint.setX(std::min(m_xmin + 0.000001, std::max(m_xmin, orig)));

    if(current_x >= m_xmax)
    {
      // If xmax is the max of the whole curve and we are not bounded,
      // we ignore.
      if(!(!bounded && current_x >= m_xLastPoint))
        m_state->currentPoint.setX(
            std::max(m_xmax - 0.000001, std::min(m_xmax, orig)));
    }
  }
}

void CommandObjectBase::submit(const std::vector<SegmentData>& segments)
{
  m_dispatcher.submit(m_model, segments);
}

void checkValidity(std::span<const SegmentData> segts)
{
#if defined(SCORE_DEBUG)
  ossia::hash_map<int32_t, const SegmentData*> by_id;
  by_id.reserve(segts.size());
  for(const auto& s : segts)
  {
    SCORE_ASSERT(ossia::safe_isfinite(s.start.x()));
    SCORE_ASSERT(ossia::safe_isfinite(s.start.y()));
    SCORE_ASSERT(ossia::safe_isfinite(s.end.x()));
    SCORE_ASSERT(ossia::safe_isfinite(s.end.y()));
    SCORE_ASSERT(by_id.emplace(s.id.val(), &s).second);
  }

  // Mutual links between distinct segments: each has at most one previous and
  // one following.
  for(const auto& s : segts)
  {
    if(s.previous)
    {
      SCORE_ASSERT(s.previous != s.id);
      auto it = by_id.find(s.previous->val());
      SCORE_ASSERT(it != by_id.end());
      SCORE_ASSERT(it->second->following == s.id);
    }
    if(s.following)
    {
      SCORE_ASSERT(s.following != s.id);
      auto it = by_id.find(s.following->val());
      SCORE_ASSERT(it != by_id.end());
      SCORE_ASSERT(it->second->previous == s.id);
    }
  }

  // No two segments overlap, zero-width ones included.
  std::vector<const SegmentData*> sorted;
  sorted.reserve(segts.size());
  for(const auto& s : segts)
    sorted.push_back(&s);
  std::sort(sorted.begin(), sorted.end(), [](auto a, auto b) {
    return a->start.x() < b->start.x();
  });
  const SegmentData* furthest{};
  for(auto s : sorted)
  {
    if(furthest)
      SCORE_ASSERT(!(s->start.x() < furthest->end.x() && furthest->start.x() < s->end.x()));
    if(!furthest || s->end.x() > furthest->end.x())
      furthest = s;
  }
#endif
}

void createPointAt(std::vector<SegmentData>& segments, Curve::Point pt)
{
  // The segment under pt, and those ending or starting exactly at it.
  SegmentData* middle = nullptr;
  SegmentData* exactBefore = nullptr;
  SegmentData* exactAfter = nullptr;
  const auto current_x = pt.x();
  for(auto& segment : segments)
  {
    if(segment.start.x() < current_x && current_x < segment.end.x())
      middle = &segment;
    if(segment.end.x() == current_x)
      exactBefore = &segment;
    if(segment.start.x() == current_x)
      exactAfter = &segment;
  }

  // Handle creation on an exact other point
  if(exactBefore || exactAfter)
  {
    if(exactBefore)
    {
      exactBefore->end = pt;
    }
    if(exactAfter)
    {
      exactAfter->start = pt;
    }
  }
  else if(middle)
  {
    // The segment goes in the first half of "middle"
    SegmentData newSegment{
        getSegmentId(segments),     middle->start, middle->end,
        middle->previous,           middle->id,    middle->type,
        middle->specificSegmentData};

    auto prev_it = ossia::find_if(segments, [&](const SegmentData& seg) {
      return seg.id == middle->previous;
    });
    if(prev_it != segments.end())
    {
      (*prev_it).following = newSegment.id;
    }

    // Both halves keep their shape where it was: a point array is cropped.
    setSegmentExtent(newSegment, middle->start, pt);
    setSegmentExtent(*middle, pt, middle->end);
    middle->previous = newSegment.id;
    segments.push_back(newSegment);
  }
  else
  {
    // The references to segments.back() below must survive both push_back.
    segments.reserve(segments.size() + 2);

    double seg_closest_from_left_x = 0;
    SegmentData* seg_closest_from_left{};
    double seg_closest_from_right_x = 1.;
    SegmentData* seg_closest_from_right{};
    for(SegmentData& segment : segments)
    {
      auto seg_start_x = segment.start.x();
      if(seg_start_x > current_x && seg_start_x < seg_closest_from_right_x)
      {
        seg_closest_from_right_x = seg_start_x;
        seg_closest_from_right = &segment;
      }

      auto seg_end_x = segment.end.x();
      if(seg_end_x < current_x && seg_end_x > seg_closest_from_left_x)
      {
        seg_closest_from_left_x = seg_end_x;
        seg_closest_from_left = &segment;
      }
    }

    // Create a curve segment for the left
    // Pushed right away: the next getSegmentId must see its id.
    {
      SegmentData newLeftSegment;
      newLeftSegment.id = getSegmentId(segments);
      segments.push_back(newLeftSegment);
    }
    SegmentData& newLeftSegment = segments.back();
    newLeftSegment.type = Metadata<ConcreteKey_k, PowerSegment>::get();
    newLeftSegment.specificSegmentData
        = QVariant::fromValue(PowerSegmentData{PowerSegmentData::linearGamma});
    newLeftSegment.start = {seg_closest_from_left_x, 0.};
    newLeftSegment.end = pt;

    if(seg_closest_from_left)
    {
      newLeftSegment.start = seg_closest_from_left->end;
      newLeftSegment.previous = seg_closest_from_left->id;

      seg_closest_from_left->following = newLeftSegment.id;
    }

    // Create a curve segment for the right
    // If we are before 1.0 we wrap to 1.0.
    if(current_x <= 1.0 || seg_closest_from_right)
    {
      {
        SegmentData newRightSegment;
        newRightSegment.id = getSegmentId(segments);
        segments.push_back(newRightSegment);
      }
      SegmentData& newRightSegment = segments.back();
      newRightSegment.type = Metadata<ConcreteKey_k, PowerSegment>::get();
      newRightSegment.specificSegmentData
          = QVariant::fromValue(PowerSegmentData{PowerSegmentData::linearGamma});
      newRightSegment.start = pt;
      newRightSegment.end = {seg_closest_from_right_x, 0.};

      newLeftSegment.following = newRightSegment.id;
      newRightSegment.previous = newLeftSegment.id;

      if(seg_closest_from_right)
      {
        newRightSegment.end = seg_closest_from_right->start;
        newRightSegment.following = seg_closest_from_right->id;

        seg_closest_from_right->previous = newRightSegment.id;
      }
    }
  }
}

std::vector<SegmentData> removeSegments(
    const Model& model, const ossia::hash_set<int32_t>& removed, bool fill)
{
  auto segs = model.toCurveData();

  // The ends of the whole curve: those of other chains are holes, filled below.
  double x0 = 0, y0 = 0, x1 = 1, y1 = 1;
  bool firstRemoved = false, lastRemoved = false;
  if(!segs.empty())
  {
    if(removed.contains(segs.front().id.val()))
    {
      firstRemoved = true;
      x0 = segs.front().start.x();
      y0 = segs.front().start.y();
    }
    if(removed.contains(segs.back().id.val()))
    {
      lastRemoved = true;
      x1 = segs.back().end.x();
      y1 = segs.back().end.y();
    }
  }

  std::erase_if(segs, [&](const SegmentData& s) { return removed.contains(s.id.val()); });
  for(auto& s : segs)
  {
    if(s.previous && removed.contains(s.previous->val()))
      s.previous = std::nullopt;
    if(s.following && removed.contains(s.following->val()))
      s.following = std::nullopt;
  }

  if(!fill)
    return segs;

  // In chain order, which is x order: a sort by x could put a vertical step
  // after its follower.
  SegmentIdAllocator ids{segs};
  auto make = [&](Curve::Point a, Curve::Point b) {
    SegmentData d;
    d.id = ids.next();
    d.start = a;
    d.end = b;
    d.type = Metadata<ConcreteKey_k, DefaultCurveSegmentModel>::get();
    d.specificSegmentData = QVariant::fromValue(DefaultCurveSegmentData{});
    return d;
  };

  if(segs.empty())
  {
    segs.push_back(make({0., y0}, {1., y1}));
    return segs;
  }

  std::vector<SegmentData> out;
  out.reserve(2 * segs.size() + 1);
  if(firstRemoved)
  {
    auto d = make({x0, y0}, segs.front().start);
    d.following = segs.front().id;
    segs.front().previous = d.id;
    out.push_back(std::move(d));
  }

  const std::size_t n = segs.size();
  for(std::size_t i = 0; i < n; i++)
  {
    out.push_back(std::move(segs[i]));
    if(i + 1 < n && !out.back().following)
    {
      auto& prev = out.back();
      auto d = make(prev.end, segs[i + 1].start);
      d.previous = prev.id;
      d.following = segs[i + 1].id;
      prev.following = d.id;
      segs[i + 1].previous = d.id;
      out.push_back(std::move(d));
    }
  }

  if(lastRemoved)
  {
    auto& last = out.back();
    auto d = make(last.end, {x1, y1});
    d.previous = last.id;
    last.following = d.id;
    out.push_back(std::move(d));
  }
  return out;
}
}
