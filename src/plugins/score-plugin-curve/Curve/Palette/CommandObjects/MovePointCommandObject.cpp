// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MovePointCommandObject.hpp"

#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveView.hpp>
#include <Curve/Palette/CommandObjects/CurveCommandObjectBase.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Process/CurveProcessModel.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/math/safe_math.hpp>

#include <boost/operators.hpp>

#include <QPoint>

#include <cmath>
#include <vector>

namespace score
{
class CommandStackFacade;
} // namespace score

namespace Curve
{

class SegmentModel;
MovePointCommandObject::MovePointCommandObject(
    const Model& model, Presenter* presenter, const score::CommandStackFacade& stack)
    : CommandObjectBase{model, presenter, stack}
{
}

MovePointCommandObject::~MovePointCommandObject() { }

static QString getPrettyText(QPointF pt, Curve::Presenter& p) noexcept
{
  if(auto parent = qobject_cast<Curve::CurveProcessModel*>(p.model().parent()))
  {
    return parent->prettyValue(pt.x(), pt.y());
  }
  return {};
}

namespace
{
auto findSegment(std::vector<SegmentData>& segments, const OptionalId<SegmentModel>& id)
{
  if(!id)
    return segments.end();
  return ossia::find_if(segments, [&](const SegmentData& s) { return s.id == *id; });
}

//! Removes the segments in `ids` and the links to them, in one pass.
void eraseSegments(std::vector<SegmentData>& segments, const ossia::hash_set<int32_t>& ids)
{
  std::erase_if(segments, [&](const SegmentData& s) { return ids.contains(s.id.val()); });
  for(auto& s : segments)
  {
    if(s.previous && ids.contains(s.previous->val()))
      s.previous = std::nullopt;
    if(s.following && ids.contains(s.following->val()))
      s.following = std::nullopt;
  }
}

// By value: callers pass the id of an element of `segments`, which erase_if moves.
void eraseSegment(std::vector<SegmentData>& segments, const Id<SegmentModel> id)
{
  std::erase_if(segments, [&](const SegmentData& s) { return s.id == id; });
  for(auto& s : segments)
  {
    if(s.previous == id)
      s.previous = std::nullopt;
    if(s.following == id)
      s.following = std::nullopt;
  }
}
}

void MovePointCommandObject::on_press()
{
  // The clicked point may be gone: the curve can change between the click and
  // the moment the state machine gets to it.
  const auto& pts = m_model.points();
  auto clickedCurvePoint_it = std::find_if(pts.begin(), pts.end(), [&](PointModel* pt) {
    return pt->previous() == m_state->clickedPointId.previous
           && pt->following() == m_state->clickedPointId.following;
  });

  m_pressed = clickedCurvePoint_it != pts.end();
  if(!m_pressed)
    return;

  auto clickedCurvePoint = *clickedCurvePoint_it;
  m_originalPress = clickedCurvePoint->pos();

  // Compute xmin, xmax
  // Look for the next and previous points
  for(PointModel* pt : m_model.points())
  {
    auto pt_x = pt->pos().x();
    if(pt == clickedCurvePoint)
      continue;

    if(pt_x >= m_xmin && pt_x < m_originalPress.x())
    {
      m_xmin = pt_x;
    }
    if(pt_x <= m_xmax && pt_x > m_originalPress.x())
    {
      m_xmax = pt_x;
    }
    if(pt_x >= m_xLastPoint)
    {
      m_xLastPoint = pt_x;
    }
  }

  // The neighbours bound the point even when they share its x.
  m_prevIndex = -1;
  m_follIndex = -1;
  m_touched.clear();
  for(std::size_t i = 0; i < m_startSegments.size(); i++)
  {
    const auto& seg = m_startSegments[i];
    if(seg.id == m_state->clickedPointId.previous)
    {
      m_xmin = std::max(m_xmin, seg.start.x());
      m_prevIndex = int64_t(i);
    }
    if(seg.id == m_state->clickedPointId.following)
    {
      m_xmax = std::min(m_xmax, seg.end.x());
      m_follIndex = int64_t(i);
    }
  }
  m_innerMin = m_xmin;
  m_innerMax = m_xmax;

  setTooltip(m_originalPress);
}

void MovePointCommandObject::move()
{
  if(!m_pressed)
    return;

  handleLocking();

  const auto cur = m_state->currentPoint;
  if(!ossia::safe_isfinite(cur.x()) || !ossia::safe_isfinite(cur.y()))
    return;

  bool ok = true;
  if(!crosses(cur.x()))
  {
    // Only the two segments around the point move: when the last move left
    // m_segments aligned with the start, restoring those two is enough.
    if(m_touched.empty() || m_segments.size() != m_startSegments.size())
      m_segments = m_startSegments;
    else
      for(auto i : m_touched)
        m_segments[i] = m_startSegments[i];
    m_touched.clear();
    setCurrentPoint(m_segments);
  }
  else
  {
    m_segments = m_startSegments;
    m_touched.clear();
    if(m_presenter && m_presenter->editionSettings().suppressOnOverlap())
      ok = suppressOverlapped(m_segments);
    else
      ok = crossOverlapped(m_segments);
  }

  if(!ok)
    return;

  checkValidity(m_segments);
  submit(m_segments);
  setTooltip(cur);
}

void MovePointCommandObject::release()
{
  if(m_pressed)
    m_dispatcher.commit();
  m_pressed = false;
  unsetTooltip();
}

void MovePointCommandObject::cancel()
{
  m_dispatcher.rollback();
  m_pressed = false;
  unsetTooltip();
}

bool MovePointCommandObject::crosses(double x) const
{
  const double orig = m_originalPress.x();
  if(x == orig)
    return false;
  // Strictly between the nearest points on each side: nothing to cross.
  if(m_innerMin < x && x < m_innerMax)
    return false;

  const auto& clicked = m_state->clickedPointId;
  const bool right = x > orig;
  auto passed
      = [&](double px) { return right ? (px > orig && px <= x) : (px < orig && px >= x); };

  for(const auto& s : m_startSegments)
  {
    // Every point but the clicked one.
    if(s.id != clicked.following && passed(s.start.x()))
      return true;
    if(s.id != clicked.previous && passed(s.end.x()))
      return true;

    // A neighbour at the same x, across a vertical step, is on the side of
    // the segment that leads to it.
    if(right && s.id == clicked.following && s.end.x() == orig)
      return true;
    if(!right && s.id == clicked.previous && s.start.x() == orig)
      return true;
  }
  return false;
}

void MovePointCommandObject::setCurrentPoint(std::vector<SegmentData>& segments)
{
  if(m_prevIndex >= 0)
  {
    segments[m_prevIndex].end = m_state->currentPoint;
    m_touched.push_back(m_prevIndex);
  }
  if(m_follIndex >= 0)
  {
    segments[m_follIndex].start = m_state->currentPoint;
    m_touched.push_back(m_follIndex);
  }
}

// Every point passed over is removed along with its segments; the moved point
// joins the segment it lands in.
bool MovePointCommandObject::suppressOverlapped(std::vector<SegmentData>& segments) const
{
  const auto cur = m_state->currentPoint;
  const double orig = m_originalPress.x();
  const bool right = cur.x() > orig;
  const auto& clicked = m_state->clickedPointId;

  static ossia::hash_set<int32_t> passed;
  passed.clear();
  for(const auto& s : segments)
  {
    if(s.id == (right ? clicked.previous : clicked.following))
      continue;
    if(right ? (s.start.x() >= orig && s.end.x() <= cur.x())
             : (s.end.x() <= orig && s.start.x() >= cur.x()))
      passed.insert(s.id.val());
  }
  eraseSegments(segments, passed);

  auto landed = ossia::find_if(segments, [&](const SegmentData& s) {
    if(s.id == (right ? clicked.previous : clicked.following))
      return false;
    return right ? (s.start.x() >= orig && s.start.x() <= cur.x() && s.end.x() > cur.x())
                 : (s.end.x() <= orig && s.end.x() >= cur.x() && s.start.x() < cur.x());
  });

  // The segment that stays attached on the side we come from.
  auto kept = findSegment(segments, right ? clicked.previous : clicked.following);
  if(kept == segments.end() && landed == segments.end())
    return false;

  const auto landed_id = landed != segments.end()
                             ? OptionalId<SegmentModel>{landed->id}
                             : OptionalId<SegmentModel>{};
  const auto kept_id = kept != segments.end() ? OptionalId<SegmentModel>{kept->id}
                                              : OptionalId<SegmentModel>{};
  if(right)
  {
    if(kept != segments.end())
    {
      kept->end = cur;
      kept->following = landed_id;
    }
    if(landed != segments.end())
    {
      landed->start = cur;
      landed->previous = kept_id;
    }
  }
  else
  {
    if(kept != segments.end())
    {
      kept->start = cur;
      kept->previous = landed_id;
    }
    if(landed != segments.end())
    {
      landed->end = cur;
      landed->following = kept_id;
    }
  }
  return true;
}

// The point is taken out of where it was, merging its two segments, and
// created again where the cursor is.
bool MovePointCommandObject::crossOverlapped(std::vector<SegmentData>& segments) const
{
  const auto& clicked = m_state->clickedPointId;
  auto prev = findSegment(segments, clicked.previous);
  auto foll = findSegment(segments, clicked.following);

  if(prev != segments.end() && foll != segments.end())
  {
    prev->end = foll->end;
    prev->following = foll->following;
    const auto prev_id = prev->id;
    if(auto next = findSegment(segments, foll->following); next != segments.end())
      next->previous = prev_id;
    std::erase_if(segments, [&](const SegmentData& s) { return s.id == *clicked.following; });
  }
  else if(prev != segments.end())
  {
    eraseSegment(segments, prev->id);
  }
  else if(foll != segments.end())
  {
    eraseSegment(segments, foll->id);
  }
  else
  {
    return false;
  }

  if(segments.empty())
    return false;

  createPointAt(segments, m_state->currentPoint);
  return true;
}

void MovePointCommandObject::setTooltip(const Point& p)
{
  if(!m_presenter)
    return;

  m_presenter->view().setValueTooltip(p, getPrettyText(p, *m_presenter));
}
void MovePointCommandObject::unsetTooltip()
{
  if(!m_presenter)
    return;

  m_presenter->view().setValueTooltip({}, {});
}
}
