// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MoveSegmentCommandObject.hpp"

#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>

#include <ossia/math/safe_math.hpp>

#include <algorithm>
#include <limits>

namespace Curve
{
MoveSegmentCommandObject::MoveSegmentCommandObject(
    const Model& model, Presenter* presenter, const score::CommandStackFacade& stack)
    : CommandObjectBase{model, presenter, stack}
{
}

MoveSegmentCommandObject::~MoveSegmentCommandObject() { }

void MoveSegmentCommandObject::on_press()
{
  m_moved = false;
  m_movedIds.clear();
  const auto clicked = m_state->clickedSegmentId;
  const auto clicked_it = find(m_startSegments, clicked);
  m_pressed = clicked_it != m_startSegments.end();
  if(!m_pressed)
    return;

  // The selection goes along when the segment pressed is part of it
  const auto& segs = m_model.segments();
  auto clicked_model = segs.find(clicked);
  if(clicked_model != segs.end() && clicked_model->selection.get())
  {
    for(const SegmentModel& s : segs)
      if(s.selection.get())
        m_movedIds.insert(s.id().val());
  }
  else
  {
    m_movedIds.insert(clicked.val());
  }

  m_originalPress = m_state->currentPoint;

  // The points which move: those of the moved segments. The others stay.
  double movedXmin = std::numeric_limits<double>::max();
  double movedXmax = std::numeric_limits<double>::lowest();
  double movedYmin = movedXmin, movedYmax = movedXmax;
  auto addMoved = [&](Curve::Point p) {
    movedXmin = std::min(movedXmin, p.x());
    movedXmax = std::max(movedXmax, p.x());
    movedYmin = std::min(movedYmin, p.y());
    movedYmax = std::max(movedYmax, p.y());
  };
  for(const auto& s : m_startSegments)
    if(m_movedIds.contains(s.id.val()))
    {
      addMoved(s.start);
      addMoved(s.end);
    }

  // A point stays when no moved segment has it
  auto moves_start = [&](const SegmentData& s) {
    return m_movedIds.contains(s.id.val())
           || (s.previous && m_movedIds.contains(s.previous->val()));
  };
  auto moves_end = [&](const SegmentData& s) {
    return m_movedIds.contains(s.id.val())
           || (s.following && m_movedIds.contains(s.following->val()));
  };

  const bool bounded = m_presenter ? m_presenter->boundedMove() : true;
  m_dxMin = -movedXmin;
  m_dxMax = bounded ? 1. - movedXmax : std::numeric_limits<double>::max();
  m_dyMin = -movedYmin;
  m_dyMax = 1. - movedYmax;
  bool fixedInside = false;
  auto addFixed = [&](double x) {
    if(x <= movedXmin)
      m_dxMin = std::max(m_dxMin, x - movedXmin);
    else if(x >= movedXmax)
      m_dxMax = std::min(m_dxMax, x - movedXmax);
    else
      fixedInside = true;
  };
  for(const auto& s : m_startSegments)
  {
    if(!moves_start(s))
      addFixed(s.start.x());
    if(!moves_end(s))
      addFixed(s.end.x());
  }
  // A point which stays between moved ones: they can only move in value
  if(fixedInside)
    m_dxMin = m_dxMax = 0.;
  m_dxMin = std::min(m_dxMin, 0.);
  m_dxMax = std::max(m_dxMax, 0.);
  m_dyMin = std::min(m_dyMin, 0.);
  m_dyMax = std::max(m_dyMax, 0.);
}

void MoveSegmentCommandObject::move()
{
  if(!m_pressed)
    return;

  const auto cur = m_state->currentPoint;
  if(!ossia::safe_isfinite(cur.x()) || !ossia::safe_isfinite(cur.y()))
    return;

  const double dx = std::clamp(cur.x() - m_originalPress.x(), m_dxMin, m_dxMax);
  const double dy = std::clamp(cur.y() - m_originalPress.y(), m_dyMin, m_dyMax);
  const QPointF d{dx, dy};

  m_segments = m_startSegments;
  for(auto& s : m_segments)
  {
    const bool moved = m_movedIds.contains(s.id.val());
    if(moved || (s.previous && m_movedIds.contains(s.previous->val())))
      s.start += d;
    if(moved || (s.following && m_movedIds.contains(s.following->val())))
      s.end += d;
  }

  checkValidity(m_segments);
  submit(m_segments);
  m_moved = true;
}

void MoveSegmentCommandObject::release()
{
  if(m_pressed && m_moved)
    m_dispatcher.commit();
  else
    m_dispatcher.rollback();
  m_pressed = false;
}

void MoveSegmentCommandObject::cancel()
{
  m_dispatcher.rollback();
  m_pressed = false;
}
}
