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

#include <multi_index/hashed_index.hpp>
#include <multi_index/mem_fun.hpp>
#include <multi_index/member.hpp>
#include <multi_index_container.hpp>

#include <vector>

namespace bmi = multi_index;
namespace score
{
class CommandStackFacade;
} // namespace score

namespace Curve
{
struct CurveSegmentMap : bmi::multi_index_container<
                             SegmentData,
                             bmi::indexed_by<bmi::hashed_unique<
                                 bmi::member<SegmentData, Id<SegmentModel>, &SegmentData::id>,
                                 CurveDataHash>>>
{
  using multi_index_container::multi_index_container;
};
class SegmentModel;
MovePointCommandObject::MovePointCommandObject(
    Presenter* presenter,
    const score::CommandStackFacade& stack)
    : CommandObjectBase{presenter, stack}
{
}

MovePointCommandObject::~MovePointCommandObject() { }

static QString getPrettyText(QPointF pt, Curve::Presenter& p) noexcept
{
  return static_cast<Curve::CurveProcessModel*>(p.model().parent())->prettyValue(pt.x(), pt.y());
}
void MovePointCommandObject::on_press()
{
  // Save the start data.
  // Firts we take the exact position of the point we clicked.
  auto clickedCurvePoint_it = std::find_if(
      m_presenter->model().points().begin(),
      m_presenter->model().points().end(),
      [&](PointModel* pt) {
        return pt->previous() == m_state->clickedPointId.previous
               && pt->following() == m_state->clickedPointId.following;
      });

  SCORE_ASSERT(clickedCurvePoint_it != m_presenter->model().points().end());
  auto clickedCurvePoint = *clickedCurvePoint_it;
  m_originalPress = clickedCurvePoint->pos();

  // Compute xmin, xmax
  // Look for the next and previous points
  for (PointModel* pt : m_presenter->model().points())
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

  m_presenter->view().setValueTooltip(
      m_originalPress, getPrettyText(m_originalPress, *m_presenter));
}

void MovePointCommandObject::move()
{
  // We start from a clean state
  CurveSegmentMap segments(m_startSegments.cbegin(), m_startSegments.cend());

  // Locking between bounds
  handleLocking();

  // Manage point - segment replacement
  handlePointOverlap(segments);

  // This handles what happens when we cross another point.
  if (m_presenter->editionSettings().suppressOnOverlap())
  {
    handleSuppressOnOverlap(segments);
  }
  else
  {
    handleCrossOnOverlap(segments);
  }

  // Rewirte and make a command
  submit(std::vector<SegmentData>(segments.begin(), segments.end()));
  m_presenter->view().setValueTooltip(
      m_state->currentPoint, getPrettyText(m_state->currentPoint, *m_presenter));
}

void MovePointCommandObject::release()
{
  m_dispatcher.commit();
  m_presenter->view().setValueTooltip({}, QString{});
}

void MovePointCommandObject::cancel()
{
  m_dispatcher.rollback();
  m_presenter->view().setValueTooltip({}, QString{});
}

void MovePointCommandObject::handlePointOverlap(CurveSegmentMap& segments)
{
  double current_x = m_state->currentPoint.x();
  // In all cases, if we're going on the same position that any other point,
  // this other point is removed and we replace it.

  auto& segments_by_id = segments.get<Segments::Hashed>();
  for (auto it = segments_by_id.begin(); it != segments_by_id.end(); ++it)
  {
    const auto& segment = *it;
    if (segment.start.x() == current_x)
    {
      segments_by_id.modify(it, [&](auto& seg) { seg.start = m_state->currentPoint; });
    }

    if (segment.end.x() == current_x)
    {
      segments_by_id.modify(it, [&](auto& seg) { seg.end = m_state->currentPoint; });
    }
  }
}

void MovePointCommandObject::handleSuppressOnOverlap(CurveSegmentMap& segments)
{
  double current_x = m_state->currentPoint.x();

  // All segments contained between the starting position and current position
  // are removed.
  // Only the starting segment perdures (or no segment if there was none.).

  auto& segments_by_id = segments.get<Segments::Hashed>();

  std::vector<Id<SegmentModel>> indicesToRemove;
  bool removed_first{};
  bool removed_last{};
  bool must_set_at_end{true};

  auto markForRemoval = [&] (const auto& segment) {
    indicesToRemove.push_back(segment.id);
    if(!segment.previous)
      removed_first = true;
    if(!segment.following)
      removed_last = true;
  };
  // First the case where we're going to the right.
  if (m_originalPress.x() <= current_x)
  {
    for (auto it = segments_by_id.begin(); it != segments_by_id.end(); ++it)
    {
      const auto& segment = *it;
      auto seg_start_x = segment.start.x();
      auto seg_end_x = segment.end.x();

      if (seg_start_x >= m_originalPress.x() && seg_start_x < current_x && seg_end_x <= current_x)
      {
        // The segment is behind us, we delete it
        if(int(indicesToRemove.size()) == int(segments.size()) - 1)
        {
          must_set_at_end = false;

          // we are "removing" the last segment, switch its start / end
          segments_by_id.modify(segments_by_id.find(it->id), [&](SegmentData& seg) {
            using namespace std;
            swap(seg.start, seg.end);
            seg.end.setX(current_x);
          });
        }
        else
        {
          markForRemoval(segment);
        }
      }
      else if (seg_start_x >= m_originalPress.x() && seg_start_x < current_x)
      {
        // We're on the middle of a segment
        segments_by_id.modify(segments_by_id.find(it->id), [&](auto& seg) {
          seg.previous = m_state->clickedPointId.previous;
          seg.start = m_state->currentPoint;
        });

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
          auto seg_it = segments_by_id.find(*m_state->clickedPointId.previous);
          segments_by_id.modify(seg_it, [&](auto& seg) { seg.following = segment.id; });
        }
      }
    }
  }
  // Now the case where we're going to the left
  else if (m_originalPress.x() >= current_x)
  {
    for (auto it = segments_by_id.begin(); it != segments_by_id.end(); ++it)
    {
      const auto& segment = *it;
      auto seg_start_x = segment.start.x();
      auto seg_end_x = segment.end.x();

      if (seg_end_x <= m_originalPress.x() && seg_start_x >= current_x && seg_end_x > current_x)
      {
        // If it had previous && next, they are merged
        if (segment.previous && segment.following)
        {
          auto seg_prev_it = segments_by_id.find(*segment.previous);
          auto seg_foll_it = segments_by_id.find(*segment.following);

          segments_by_id.modify(seg_prev_it, [&](auto& seg) { seg.following = seg_foll_it->id; });
          segments_by_id.modify(seg_foll_it, [&](auto& seg) { seg.previous = seg_prev_it->id; });
        }
        else if (segment.following)
        {
          auto seg_foll_it = segments_by_id.find(*segment.following);
          segments_by_id.modify(
              seg_foll_it, [&](auto& seg) { seg.previous = OptionalId<SegmentModel>{}; });
        }
        else if (segment.previous)
        {
          auto seg_prev_it = segments_by_id.find(*segment.previous);
          segments_by_id.modify(
              seg_prev_it, [&](auto& seg) { seg.following = OptionalId<SegmentModel>{}; });
        }

        // The segment is in front of us, we delete it
        markForRemoval(segment);
      }
      else if (seg_end_x < m_originalPress.x() && seg_end_x > current_x)
      {
        segments_by_id.modify(it, [&](auto& seg) {
          seg.following = m_state->clickedPointId.following;
          seg.end = m_state->currentPoint;
        });

        if (m_state->clickedPointId.following)
        {
          // We also set the previous to the current segment if available.
          auto seg_it = segments_by_id.find(*m_state->clickedPointId.following);
          segments_by_id.modify(seg_it, [&](auto& seg) { seg.previous = segment.id; });
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
    segments_by_id.erase(elt);
  }

  if(removed_first)
  {
    auto seg_it = segments_by_id.find(*m_state->clickedPointId.following);
    SCORE_ASSERT(seg_it != segments_by_id.end());
    segments_by_id.modify(seg_it, [&](auto& seg) { seg.previous = {}; });
  }
  if(removed_last)
  {
    auto seg_it = segments_by_id.find(*m_state->clickedPointId.previous);
    SCORE_ASSERT(seg_it != segments_by_id.end());
    segments_by_id.modify(seg_it, [&](auto& seg) { seg.following = {}; });
  }

  // Then we change the start/end of the correct segments
  if(must_set_at_end)
    setCurrentPoint(segments);
}

void MovePointCommandObject::handleCrossOnOverlap(CurveSegmentMap& segments)
{
  double current_x = m_state->currentPoint.x();
  auto& segments_by_id = segments.get<Segments::Hashed>();
  // In this case we merge at the origins of the point and we create if it is
  // in a new place.

  // First, if we go to the right.
  if (current_x > m_originalPress.x())
  {
    // Get the segment we're in, if there's any
    auto middleSegmentIt = std::find_if(
        segments_by_id.begin(),
        segments_by_id.end(),
        [&](const SegmentData& segment) { // Going to the right
          return segment.start.x() > m_originalPress.x() && segment.start.x() < current_x
                 && segment.end.x() > current_x;
        });
    auto middle = middleSegmentIt != segments_by_id.end() ? &*middleSegmentIt : nullptr;

    // First part : removal of the segments around the initial click
    // If we have a following segment and the current position > end of the
    // following segment
    if (m_state->clickedPointId.following)
    {
      auto foll_seg_it = segments_by_id.find(*m_state->clickedPointId.following);
      auto& foll_seg = *foll_seg_it;
      if (current_x > foll_seg.end.x())
      {
        // If there was also a previous segment, it now goes to the end of the
        // presently removed segment.
        if (m_state->clickedPointId.previous)
        {
          auto prev_seg_it = segments_by_id.find(*m_state->clickedPointId.previous);
          segments_by_id.modify(prev_seg_it, [&](auto& seg) {
            seg.end = foll_seg.end;
            seg.following = foll_seg.following;
          });
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
          auto foll_foll_seg_it = segments_by_id.find(*foll_seg.following);
          segments_by_id.modify(foll_foll_seg_it, [&](auto& seg) {
            seg.previous = m_state->clickedPointId.previous;
          });
        }

        segments_by_id.erase(foll_seg_it);
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
      for (const auto& segment : segments)
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
          auto prev_seg_it = segments_by_id.find(*m_state->clickedPointId.previous);
          auto& prev_seg = *prev_seg_it;

          if (prev_seg.previous)
          {
            // We set its following to null.
            auto prev_prev_seg_it = segments_by_id.find(*prev_seg.previous);
            segments_by_id.modify(prev_prev_seg_it, [&](auto& seg) { seg.following = {}; });
          }

          segments_by_id.erase(prev_seg_it);
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
          getSegmentId(segments_by_id),
          middle->start,
          m_state->currentPoint,
          middle->previous,
          middle->id,
          middle->type,
          middle->specificSegmentData};

      if (middle->previous)
      {
        auto prev_it = segments_by_id.find(*middle->previous);
        // TODO we shouldn't have to test for this, only test if
        // middle->previous != id{}
        if (prev_it != segments_by_id.end())
        {
          segments_by_id.modify(prev_it, [&](auto& seg) { seg.following = newSegment.id; });
        }
      }

      segments_by_id.modify(middleSegmentIt, [&](auto& seg) {
        seg.start = m_state->currentPoint;
        seg.previous = newSegment.id;
      });

      segments.insert(newSegment);
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
        segments_by_id.begin(),
        segments_by_id.end(),
        [&](const auto& segment) { // Going to the left
          return segment.end.x() < m_originalPress.x() && segment.start.x() < current_x
                 && segment.end.x() > current_x;
        });
    auto middle = middleSegmentIt != segments_by_id.end() ? &*middleSegmentIt : nullptr;

    // First part : removal of the segments around the initial click
    // If we have a following segment and the current position > end of the
    // following segment
    if (m_state->clickedPointId.previous)
    {
      auto prev_seg_it = segments_by_id.find(*m_state->clickedPointId.previous);
      auto& prev_seg = *prev_seg_it;
      if (current_x < prev_seg.start.x())
      {
        // If there was also a following segment to the click, it now goes to
        // the start of the
        // presently removed segment.
        if (m_state->clickedPointId.following)
        {
          auto foll_seg_it = segments_by_id.find(*m_state->clickedPointId.following);
          segments_by_id.modify(foll_seg_it, [&](auto& seg) {
            seg.start = prev_seg.start;
            seg.previous = prev_seg.previous;
          });
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
          auto prev_prev_seg_it = segments_by_id.find(*prev_seg.previous);
          segments_by_id.modify(prev_prev_seg_it, [&](auto& seg) {
            seg.following = m_state->clickedPointId.following;
          });
        }

        segments_by_id.erase(prev_seg_it);
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
      for (const auto& segment : segments)
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
        auto foll_seg_it = segments_by_id.find(*m_state->clickedPointId.following);
        auto& foll_seg = *foll_seg_it;
        if (foll_seg.following)
        {
          // We set its following to null.
          auto foll_foll_seg_it = segments_by_id.find(*foll_seg.following);
          segments_by_id.modify(
              foll_foll_seg_it, [&](auto& seg) { seg.previous = OptionalId<SegmentModel>{}; });
        }

        segments_by_id.erase(foll_seg_it);
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
          getSegmentId(segments_by_id),
          m_state->currentPoint,
          middle->end,
          middle->id,
          middle->following,
          middle->type,
          middle->specificSegmentData};

      if (middle->following)
      {
        auto foll_it = segments_by_id.find(*middle->following);
        // TODO we shouldn't have to test for this, only test if
        // middle->previous != id{}
        if (foll_it != segments_by_id.end())
        {
          segments_by_id.modify(foll_it, [&](auto& seg) { seg.previous = newSegment.id; });
        }
      }

      segments_by_id.modify(middleSegmentIt, [&](auto& seg) {
        seg.end = m_state->currentPoint;
        seg.following = newSegment.id;
      });

      segments_by_id.insert(newSegment);
    }
    else
    {
      // TODO (see the commented block on the symmetric part)
    }
  }
}

void MovePointCommandObject::setCurrentPoint(CurveSegmentMap& segments)
{
  auto& segments_by_id = segments.get<Segments::Hashed>();

  if (m_state->clickedPointId.previous)
  {
    auto seg_prev_it = segments_by_id.find(*m_state->clickedPointId.previous);
    if (seg_prev_it != segments_by_id.end())
    {
      segments_by_id.modify(seg_prev_it, [&](auto& seg) { seg.end = m_state->currentPoint; });
    }
  }

  if (m_state->clickedPointId.following)
  {
    auto seg_foll_it = segments_by_id.find(*m_state->clickedPointId.following);
    if (seg_foll_it != segments_by_id.end())
    {
      segments_by_id.modify(seg_foll_it, [&](auto& seg) { seg.start = m_state->currentPoint; });
    }
  }
}
}
