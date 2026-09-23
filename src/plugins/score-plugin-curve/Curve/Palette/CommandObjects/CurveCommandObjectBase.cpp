// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveCommandObjectBase.hpp"

#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePoint.hpp>

#include <Curve/Segment/Power/PowerSegment.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QVariant>

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
  m_startSegments = m_model.toCurveData();
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

void CommandObjectBase::submit(std::vector<SegmentData>&& segments)
{
  m_dispatcher.submit(m_model, std::move(segments));
}

void checkValidity(SegmentMapImpl& segts)
{
#if defined(SCORE_DEBUG)
  for(auto& seg : segts)
  {
    auto& id = seg.first;
    auto& s = seg.second;
    SCORE_ASSERT(std::isfinite(s.start.x()));
    SCORE_ASSERT(std::isfinite(s.start.y()));
    SCORE_ASSERT(std::isfinite(s.end.x()));
    SCORE_ASSERT(std::isfinite(s.end.y()));
    SCORE_ASSERT(id == s.id);
    if(s.previous)
    {
      SCORE_ASSERT(s.previous != s.id);
      bool ok = ossia::any_of(
          segts, [&s](auto& rhs) { return *s.previous == rhs.second.id; });
      SCORE_ASSERT(ok);
    }
    if(s.following)
    {
      SCORE_ASSERT(s.following != s.id);
      bool ok = ossia::any_of(
          segts, [&s](auto& rhs) { return *s.following == rhs.second.id; });
      SCORE_ASSERT(ok);
    }

    auto num_prev = std::count_if(segts.begin(), segts.end(), [&](auto& rhs) {
      return s.id == rhs.second.following;
    });
    SCORE_ASSERT(num_prev == 0 || num_prev == 1);
    auto num_foll = std::count_if(segts.begin(), segts.end(), [&](auto& rhs) {
      return s.id == rhs.second.previous;
    });
    SCORE_ASSERT(num_foll == 0 || num_foll == 1);
  }

  for(auto& [i1, s1] : segts)
  {
    for(auto& [i2, s2] : segts)
    {
      if(s1.id != s2.id)
      {
        SCORE_ASSERT(!(s1.start.x() < s2.end.x() && s2.start.x() < s1.end.x()));
      }
    }
  }
#endif
}

void checkValidity(std::span<SegmentData> segts)
{
#if defined(SCORE_DEBUG)
  for(auto& s : segts)
  {
    SCORE_ASSERT(std::isfinite(s.start.x()));
    SCORE_ASSERT(std::isfinite(s.start.y()));
    SCORE_ASSERT(std::isfinite(s.end.x()));
    SCORE_ASSERT(std::isfinite(s.end.y()));
    if(s.previous)
    {
      SCORE_ASSERT(s.previous != s.id);
      bool ok = ossia::any_of(
          segts, [&s](SegmentData& rhs) { return *s.previous == rhs.id; });
      SCORE_ASSERT(ok);
    }
    if(s.following)
    {
      SCORE_ASSERT(s.following != s.id);
      bool ok = ossia::any_of(
          segts, [&s](SegmentData& rhs) { return *s.following == rhs.id; });
      SCORE_ASSERT(ok);
    }

    auto num_prev = std::count_if(segts.begin(), segts.end(), [&](SegmentData& rhs) {
      return s.id == rhs.following;
    });
    SCORE_ASSERT(num_prev == 0 || num_prev == 1);
    auto num_foll = std::count_if(segts.begin(), segts.end(), [&](SegmentData& rhs) {
      return s.id == rhs.previous;
    });
    SCORE_ASSERT(num_foll == 0 || num_foll == 1);
  }

  for(auto& s1 : segts)
  {
    for(auto& s2 : segts)
    {
      if(s1.following == s2.id)
        SCORE_ASSERT(s2.previous == s1.id);
      if(s1.id == s2.previous)
        SCORE_ASSERT(s2.id == s1.following);
      if(s1.id != s2.id)
      {
        SCORE_ASSERT(!(s1.start.x() < s2.end.x() && s2.start.x() < s1.end.x()));
      }
    }
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
        getSegmentId(segments),     middle->start, pt,
        middle->previous,           middle->id,    middle->type,
        middle->specificSegmentData};

    auto prev_it = ossia::find_if(segments, [&](const SegmentData& seg) {
      return seg.id == middle->previous;
    });
    if(prev_it != segments.end())
    {
      (*prev_it).following = newSegment.id;
    }

    middle->start = pt;
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
}
