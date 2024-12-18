// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveCommandObjectBase.hpp"

#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePoint.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>

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
  if(m_presenter && m_presenter->editionSettings().lockBetweenPoints())
  {
    if(current_x <= m_xmin)
      m_state->currentPoint.setX(m_xmin + 0.000001);

    if(current_x >= m_xmax)
    {
      // If xmax is the max of the whole curve and we are not bounded,
      // we ignore.
      if(!(!bounded && current_x >= m_xLastPoint))
        m_state->currentPoint.setX(m_xmax - 0.000001);
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
        SCORE_ASSERT(!(s1.start.x() >= s2.start.x() && s1.start.x() < s2.end.x()));
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
        SCORE_ASSERT(!(s1.start.x() >= s2.start.x() && s1.start.x() < s2.end.x()));
      }
    }
  }
#endif
}
}
