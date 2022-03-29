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

#include <boost/operators.hpp>

#include <QPoint>

#include <ossia/detail/hash_map.hpp>
#include <vector>

namespace score
{
class CommandStackFacade;
} // namespace score

namespace Curve
{
using SegmentMapImpl = ossia::fast_hash_map<Id<SegmentModel>, SegmentData, CurveDataHash>;

struct CurveSegmentMap : SegmentMapImpl
{
  using SegmentMapImpl::SegmentMapImpl;
};
class SegmentModel;
MovePointCommandObject::MovePointCommandObject(
    const Model& model,
    Presenter* presenter,
    const score::CommandStackFacade& stack)
    : CommandObjectBase{model, presenter, stack}
{
}

MovePointCommandObject::~MovePointCommandObject() { }

static QString getPrettyText(QPointF pt, Curve::Presenter& p) noexcept
{
  return static_cast<Curve::CurveProcessModel*>(p.model().parent())
      ->prettyValue(pt.x(), pt.y());
}
void MovePointCommandObject::on_press()
{
  // Save the start data.
  // First we take the exact position of the point we clicked.
  const auto& pts = m_model.points();
  auto clickedCurvePoint_it = std::find_if(
      pts.begin(),
      pts.end(),
      [&](PointModel* pt) {
        return pt->previous() == m_state->clickedPointId.previous
               && pt->following() == m_state->clickedPointId.following;
      });

  SCORE_ASSERT(clickedCurvePoint_it != pts.end());
  auto clickedCurvePoint = *clickedCurvePoint_it;
  m_originalPress = clickedCurvePoint->pos();

  // Compute xmin, xmax
  // Look for the next and previous points
  for (PointModel* pt : m_model.points())
  {
    auto pt_x = pt->pos().x();
    if (pt == clickedCurvePoint)
      continue;

    if (pt_x >= m_xmin && pt_x < m_originalPress.x())
    {
      m_xmin = pt_x;
    }
    if (pt_x <= m_xmax && pt_x > m_originalPress.x())
    {
      m_xmax = pt_x;
    }
    if (pt_x >= m_xLastPoint)
    {
      m_xLastPoint = pt_x;
    }
  }

  setTooltip(m_originalPress);
}

void MovePointCommandObject::move()
{
  // We start from a clean state
  CurveSegmentMap segments;
  for(auto& seg : m_startSegments)
    segments.emplace(seg.id, seg);

  try
  {
    // Locking between bounds
    handleLocking();

    // Manage point - segment replacement
    handlePointOverlap(segments);

    // This handles what happens when we cross another point.
    if (m_presenter && m_presenter->editionSettings().suppressOnOverlap())
    {
      handleSuppressOnOverlap(segments);
    }
    else
    {
      handleCrossOnOverlap(segments);
    }
  }
  catch (...)
  {
    return;
  }

  // Rewrite and make a command
  std::vector<SegmentData> ret;
  ret.reserve(segments.size());
  for(auto& [id, seg] : segments)
    ret.emplace_back(std::move(seg));
  submit(std::move(ret));
  setTooltip(m_state->currentPoint);
}

void MovePointCommandObject::release()
{
  m_dispatcher.commit();
  unsetTooltip();
}

void MovePointCommandObject::cancel()
{
  m_dispatcher.rollback();
  unsetTooltip();
}

void MovePointCommandObject::handlePointOverlap(CurveSegmentMap& segments)
{
  double current_x = m_state->currentPoint.x();
  // In all cases, if we're going on the same position that any other point,
  // this other point is removed and we replace it.

  for (auto it = segments.begin(); it != segments.end(); ++it)
  {
    auto& segment = it->second;
    if (segment.start.x() == current_x)
    {
      segment.start = m_state->currentPoint;
    }

    if (segment.end.x() == current_x)
    {
      segment.end = m_state->currentPoint;
    }
  }
}

void MovePointCommandObject::handleSuppressOnOverlap(CurveSegmentMap& segments)
{
  double current_x = m_state->currentPoint.x();

  // All segments contained between the starting position and current position
  // are removed.
  // Only the starting segment perdures (or no segment if there was none.).

  std::vector<Id<SegmentModel>> indicesToRemove;
  bool removed_first{};
  bool removed_last{};
  bool must_set_at_end{true};

  auto markForRemoval = [&](const SegmentData& segment) {
    indicesToRemove.push_back(segment.id);
    if (!segment.previous)
      removed_first = true;
    if (!segment.following)
      removed_last = true;
  };
  // First the case where we're going to the right.
  if (m_originalPress.x() <= current_x)
  {
    for (auto it = segments.begin(); it != segments.end(); ++it)
    {
      auto& segment = it->second;
      auto seg_start_x = segment.start.x();
      auto seg_end_x = segment.end.x();

      if (seg_start_x >= m_originalPress.x() && seg_start_x < current_x
          && seg_end_x <= current_x)
      {
        // The segment is behind us, we delete it
        if (int(indicesToRemove.size()) == int(segments.size()) - 1)
        {
          must_set_at_end = false;

          // we are "removing" the last segment, switch its start / end
          using namespace std;
          swap(segment.start, segment.end);
          segment.end.setX(current_x);
        }
        else
        {
          markForRemoval(segment);
        }
      }
      else if (seg_start_x >= m_originalPress.x() && seg_start_x < current_x)
      {
        // We're on the middle of a segment
        segment.previous = m_state->clickedPointId.previous;
        segment.start = m_state->currentPoint;

        // If the new segment is non-sensical we remove it
        if (segment.start.x() >= segment.end.x())
        {
          markForRemoval(segment);
        }

        // The new "previous" segment becomes the previous segment of the
        // moving point.
        else if (m_state->clickedPointId.previous)
        {
          // We also set the following to the current segment if available.
          auto seg_it = segments.find(*m_state->clickedPointId.previous);
          SCORE_ASSERT(seg_it != segments.end());
          seg_it->second.following = segment.id;
        }
      }
    }
  }
  // Now the case where we're going to the left
  else if (m_originalPress.x() >= current_x)
  {
    for (auto it = segments.begin(); it != segments.end(); ++it)
    {
      auto& segment = it->second;
      auto seg_start_x = segment.start.x();
      auto seg_end_x = segment.end.x();

      if (seg_end_x <= m_originalPress.x() && seg_start_x >= current_x
          && seg_end_x > current_x)
      {
        // If it had previous && next, they are merged
        if (segment.previous && segment.following)
        {
          auto seg_prev_it = segments.find(*segment.previous);
          auto seg_foll_it = segments.find(*segment.following);

          seg_prev_it->second.following = seg_foll_it->second.id;
          seg_foll_it->second.previous = seg_prev_it->second.id;
        }
        else if (segment.following)
        {
          auto seg_foll_it = segments.find(*segment.following);
          seg_foll_it->second.previous = OptionalId<SegmentModel>{};
        }
        else if (segment.previous)
        {
          auto seg_prev_it = segments.find(*segment.previous);
          seg_prev_it->second.following = OptionalId<SegmentModel>{};
        }

        // The segment is in front of us, we delete it
        markForRemoval(segment);
      }
      else if (seg_end_x < m_originalPress.x() && seg_end_x > current_x)
      {
        segment.following = m_state->clickedPointId.following;
        segment.end = m_state->currentPoint;

        if (m_state->clickedPointId.following)
        {
          // We also set the previous to the current segment if available.
          auto seg_it
              = segments.find(*m_state->clickedPointId.following);
          seg_it->second.previous = segment.id;
        }
      }
    }
  }

  // TODO check for reversion of start/end

  // We remove what should be removed. The indices are sorted given how we add
  // them.
  // So we take them from last to first so that when removing in segments,
  // the order stays valid.
  for (const auto& elt : indicesToRemove)
  {
    segments.erase(elt);
  }

  if (removed_first)
  {
    auto seg_it = segments.find(*m_state->clickedPointId.following);
    SCORE_ASSERT(seg_it != segments.end());
    seg_it->second.previous = {};
  }
  if (removed_last)
  {
    auto seg_it = segments.find(*m_state->clickedPointId.previous);
    SCORE_ASSERT(seg_it != segments.end());
    seg_it->second.following = {};
  }

  // Then we change the start/end of the correct segments
  if (must_set_at_end)
    setCurrentPoint(segments);
}

void MovePointCommandObject::handleCrossOnOverlap(CurveSegmentMap& segments)
{
  double current_x = m_state->currentPoint.x();
  // In this case we merge at the origins of the point and we create if it is
  // in a new place.

  // First, if we go to the right.
  if (current_x > m_originalPress.x())
  {
    // Get the segment we're in, if there's any
    auto middleSegmentIt = std::find_if(
        segments.begin(),
        segments.end(),
        [&](const auto& p) { // Going to the right
          auto& segment = p.second;
          return segment.start.x() > m_originalPress.x()
                 && segment.start.x() < current_x
                 && segment.end.x() > current_x;
        });
    auto middle = middleSegmentIt != segments.end() ? &middleSegmentIt->second
                                                          : nullptr;

    // First part : removal of the segments around the initial click
    // If we have a following segment and the current position > end of the
    // following segment
    if (m_state->clickedPointId.following)
    {
      auto foll_seg_it
          = segments.find(*m_state->clickedPointId.following);
      if(foll_seg_it == segments.end())
      {
        qDebug() << "Following segment not found ?" << m_state->clickedPointId.following->val();
        throw std::runtime_error("Curve edition error");
      }
      auto& foll_seg = foll_seg_it->second;
      if (current_x >= foll_seg.end.x())
      {
        // If there was also a previous segment, it now goes to the end of the
        // presently removed segment.
        if (m_state->clickedPointId.previous)
        {
          auto prev_seg_it = segments.find(*m_state->clickedPointId.previous);
          prev_seg_it->second.end = foll_seg.end;
          prev_seg_it->second.following = foll_seg.following;
        }
        else
        {
          // Link to the previous segment, or to the zero, maybe ?
        }

        // If the one about to be deleted also had a following, we set its
        // previous
        // to the previous of the clicked point
        if (foll_seg.following)
        {
          auto foll_foll_seg_it = segments.find(*foll_seg.following);
          foll_foll_seg_it->second.previous = m_state->clickedPointId.previous;
        }

        segments.erase(foll_seg_it);
      }
      else
      {
        // We have not crossed a point
        setCurrentPoint(segments);
      }
    }
    else
    {
      // If we have crossed a point (after some "emptiness")
      bool crossed = false;
      for (const auto& [id, segment] : segments)
      {
        auto seg_start_x = segment.start.x();
        if (seg_start_x < current_x && seg_start_x > m_originalPress.x())
        {
          crossed = true;
          break;
        }
      }

      if (crossed)
      {
        // We remove the previous of the clicked point
        if (m_state->clickedPointId.previous)
        {
          auto prev_seg_it
              = segments.find(*m_state->clickedPointId.previous);
          auto& prev_seg = prev_seg_it->second;

          if (prev_seg.previous)
          {
            // We set its following to null.
            auto prev_prev_seg_it = segments.find(*prev_seg.previous);
            prev_prev_seg_it->second.following = {};
          }

          segments.erase(prev_seg_it);
        }
        else
        {
          SCORE_TODO;
        }
      }
      else
      {
        // We have not crossed a point
        setCurrentPoint(segments);
      }
    }

    // Second part : creation of a new segment where the cursor actually is
    if (middle)
    {
      // We insert a new element after the leftmost point from the current
      // point.
      // Since we are in a segment we split it and create another with a new
      // id.
      SegmentData newSegment{
          getSegmentId(segments),
          middle->start,
          m_state->currentPoint,
          middle->previous,
          middle->id,
          middle->type,
          middle->specificSegmentData};

      if (middle->previous)
      {
        auto prev_it = segments.find(*middle->previous);
        // TODO we shouldn't have to test for this, only test if
        // middle->previous != id{}
        if (prev_it != segments.end())
        {
          prev_it->second.following = newSegment.id;
        }
      }

      middleSegmentIt->second.start = m_state->currentPoint;
      middleSegmentIt->second.previous = newSegment.id;

      segments.emplace(newSegment.id, newSegment);
    }
    else
    {
      /*
      // We're on the empty; we make a new linear segment from the end of the
      last point
      // or zero if there is none ? this can't happen unless we select a point
      on zero.

      double seg_closest_from_left_x = 0;
      CurveSegmentModel* seg_closest_from_left{};
      for(CurveSegmentModel* segment : segments)
      {
          auto seg_end_x = segment.end.x();
          if(seg_end_x < current_x && seg_end_x > seg_closest_from_left_x)
          {
              seg_closest_from_left_x = seg_end_x;
              seg_closest_from_left = segment;
          }
      }
      auto newSegment = new LinearCurveSegmentModel{getStrongId(segments),
      nullptr};
      newSegment->setEnd(m_state->currentPoint);

      if(seg_closest_from_left)
      {
          newSegment->setStart(seg_closest_from_left->end());
          newSegment->setPrevious(seg_closest_from_left->id());
      }

      segments.append(newSegment);
      */
    }
  }
  else if (current_x < m_originalPress.x())
  {
    // Get the segment we're in, if there's any
    auto middleSegmentIt = std::find_if(
        segments.begin(),
        segments.end(),
        [&](const auto& p) { // Going to the left
          auto& segment = p.second;
          return segment.end.x() < m_originalPress.x()
                 && segment.start.x() < current_x
                 && segment.end.x() > current_x;
        });
    auto middle = middleSegmentIt != segments.end() ? &middleSegmentIt->second
                                                          : nullptr;

    // First part : removal of the segments around the initial click
    // If we have a following segment and the current position > end of the
    // following segment
    if (m_state->clickedPointId.previous)
    {
      auto prev_seg_it
          = segments.find(*m_state->clickedPointId.previous);
      if(prev_seg_it == segments.end())
      {
        qDebug() << "Previous segment not found ?" << m_state->clickedPointId.previous->val();
        throw std::runtime_error("Curve edition error");
      }

      auto& prev_seg = prev_seg_it->second;
      if (current_x <= prev_seg.start.x())
      {
        // If there was also a following segment to the click, it now goes to
        // the start of the
        // presently removed segment.
        if (m_state->clickedPointId.following)
        {
          auto foll_seg_it
              = segments.find(*m_state->clickedPointId.following);
          foll_seg_it->second.start = prev_seg.start;
          foll_seg_it->second.previous = prev_seg.previous;
        }
        else
        {
          // Link to the following segment, or to the zero, maybe ?
        }

        // If the one about to be deleted also had a previous, we set its
        // following
        // to the following of the clicked point
        if (prev_seg.previous)
        {
          auto prev_prev_seg_it = segments.find(*prev_seg.previous);
          prev_prev_seg_it->second.following = m_state->clickedPointId.following;
        }

        segments.erase(prev_seg_it);
      }
      else
      {
        // We have not crossed a point
        setCurrentPoint(segments);
      }
    }
    else
    {
      // If we have crossed a point (after some "emptiness")
      bool crossed = false;
      for (const auto& [id, segment] : segments)
      {
        auto seg_end_x = segment.end.x();
        if (seg_end_x > current_x && seg_end_x < m_originalPress.x())
        {
          crossed = true;
          break;
        }
      }

      if (crossed)
      {
        // We remove the following of the clicked point
        auto foll_seg_it
            = segments.find(*m_state->clickedPointId.following);
        auto& foll_seg = foll_seg_it->second;
        if (foll_seg.following)
        {
          // We set its following to null.
          auto foll_foll_seg_it = segments.find(*foll_seg.following);
          foll_foll_seg_it->second.previous = OptionalId<SegmentModel>{};
        }

        segments.erase(foll_seg_it);
      }
      else
      {
        // We have not crossed a point
        setCurrentPoint(segments);
      }
    }

    // Second part : creation of a new segment where the cursor actually is
    if (middle)
    {
      SegmentData newSegment{
          getSegmentId(segments),
          m_state->currentPoint,
          middle->end,
          middle->id,
          middle->following,
          middle->type,
          middle->specificSegmentData};

      if (middle->following)
      {
        auto foll_it = segments.find(*middle->following);
        // TODO we shouldn't have to test for this, only test if
        // middle->previous != id{}
        if (foll_it != segments.end())
        {
          foll_it->second.previous = newSegment.id;
        }
      }

      middleSegmentIt->second.end = m_state->currentPoint;
      middleSegmentIt->second.following = newSegment.id;

      segments.emplace(newSegment.id, newSegment);
    }
    else
    {
      // TODO (see the commented block on the symmetric part)
    }
  }
}

void MovePointCommandObject::setCurrentPoint(CurveSegmentMap& segments)
{
  if (m_state->clickedPointId.previous)
  {
    auto seg_prev_it = segments.find(*m_state->clickedPointId.previous);
    if (seg_prev_it != segments.end())
    {
       seg_prev_it->second.end = m_state->currentPoint;
    }
  }

  if (m_state->clickedPointId.following)
  {
    auto seg_foll_it = segments.find(*m_state->clickedPointId.following);
    if (seg_foll_it != segments.end())
    {
      seg_foll_it->second.start = m_state->currentPoint;
    }
  }
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
