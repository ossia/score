#include "MovePointCommandObject.hpp"
#include "Curve/CurvePresenter.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"
#include "Curve/Point/CurvePointModel.hpp"
#include "Curve/Point/CurvePointView.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include "Curve/Segment/Linear/LinearCurveSegmentModel.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

MovePointCommandObject::MovePointCommandObject(
        CurvePresenter* presenter,
        iscore::CommandStack& stack):
    CurveCommandObjectBase{presenter, stack}
{

}

void MovePointCommandObject::on_press()
{
    // Save the start data.
    // Firts we take the exact position of the point we clicked.
    auto clickedCurvePoint = *std::find_if(
                                 m_presenter->model().points().begin(),
                                 m_presenter->model().points().end(),
                                 [&] (CurvePointModel* pt)
    { return pt->previous()  == m_state->clickedPointId.previous
          && pt->following() == m_state->clickedPointId.following; });

    m_originalPress = clickedCurvePoint->pos();

    // Compute xmin, xmax
    // Look for the next and previous points
    for(CurvePointModel* pt : m_presenter->model().points())
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
    }
}

void MovePointCommandObject::move()
{
    // First we deserialize the base segments. This way we can start from a clean state at each time.
    // TODO it would be less costly to have a specific "data" structure for this (no need for vcalls then).
    auto segments = m_startSegments;

    // TODO: if we're locking we can just bybass the overlap ?
    // Locking between bounds
    handleLocking();

    // Manage point - segment replacement
    handlePointOverlap(segments);

    // This handles what happens when we cross another point.
    if(m_presenter->suppressOnOverlap())
    {
        handleSuppressOnOverlap(segments);
    }
    else
    {
        handleCrossOnOverlap(segments);
    }


    // Rewirte and make a command
    submit(segments);
}

void MovePointCommandObject::release()
{
    m_dispatcher.commit();
}

void MovePointCommandObject::cancel()
{
    m_dispatcher.rollback();
}

void MovePointCommandObject::handlePointOverlap(QVector<CurveSegmentData> &segments)
{
    double current_x = m_state->currentPoint.x();
    // In all cases, if we're going on the same position that any other point,
    // this other point is removed and we replace it.
    for(CurveSegmentData& segment : segments)
    {
        if(segment.start.x() == current_x)
        {
            segment.start = m_state->currentPoint;
        }

        if(segment.end.x() == current_x)
        {
            segment.end = m_state->currentPoint;
        }
    }
}

void MovePointCommandObject::handleSuppressOnOverlap(QVector<CurveSegmentData>& segments)
{
    double current_x = m_state->currentPoint.x();
    // All segments contained between the starting position and current position are removed.
    // Only the starting segment perdures (or no segment if there was none.).

    std::vector<int> indicesToRemove;
    int i = 0;
    // First the case where we're going to the right.
    if(m_originalPress.x() < current_x)
    {
        for(CurveSegmentData& segment : segments)
        {
            auto seg_start_x = segment.start.x();
            auto seg_end_x = segment.end.x();

            if(seg_start_x >= m_originalPress.x()
            && seg_start_x < current_x
            && seg_end_x < current_x)
            {
                // The segment is behind us, we delete it
                indicesToRemove.push_back(i);

                // TODO here maybe a list would be faster??? need to benchmark
            }
            else if(seg_start_x >= m_originalPress.x()
                 && seg_start_x < current_x)
            {
                // We're on the middle of a segment
                segment.previous = m_state->clickedPointId.previous;
                segment.start = m_state->currentPoint;
                // If the new segment is non-sensical we remove it
                if(segment.start.x() >= segment.end.x())
                {
                    indicesToRemove.push_back(i);
                }
                // The new "previous" segment becomes the previous segment of the moving point.
                else if(m_state->clickedPointId.previous)
                {
                    // We also set the following to the current segment if available.
                    auto it = find(segments, m_state->clickedPointId.previous);
                    (*it).following = segment.id;
                }
            }
            i++;
        }
    }
    // Now the case where we're going to the left
    else if(m_originalPress.x() > current_x)
    {
        for(CurveSegmentData& segment : segments)
        {
            auto seg_start_x = segment.start.x();
            auto seg_end_x = segment.end.x();

            if(seg_end_x <= m_originalPress.x()
            && seg_start_x > current_x
            && seg_end_x > current_x)
            {
                // If it had previous && next, they are merged
                if(segment.previous && segment.following)
                {
                    CurveSegmentData* seg_prev = nullptr;
                    CurveSegmentData* seg_foll = nullptr;
                    // OPTIMIZEME /!\ quadratic /!\ also all these find_if
                    for(auto& segment_sub : segments)
                    {
                        if(segment_sub.id == segment.previous)
                            seg_prev = &segment_sub;
                        if(segment_sub.id == segment.following)
                            seg_foll = &segment_sub;

                        if(seg_prev && seg_foll)
                            break;
                    }

                    seg_prev->following = seg_foll->id;
                    seg_foll->previous = seg_prev->id;
                }
                else if(segment.following)
                {
                    auto& seg_foll = *find(segments, segment.following);
                    seg_foll.previous = Id<CurveSegmentModel>{};
                }
                else if(segment.previous)
                {
                    auto& seg_prev = *find(segments, segment.previous);
                    seg_prev.following = Id<CurveSegmentModel>{};
                }

                // The segment is in front of us, we delete it
                indicesToRemove.push_back(i);
            }
            else if(seg_end_x < m_originalPress.x()
                 && seg_end_x > current_x)
            {
                segment.following = m_state->clickedPointId.following;
                segment.end = m_state->currentPoint;
                if(m_state->clickedPointId.following)
                {
                    // We also set the previous to the current segment if available.
                    auto& seg = *find(segments, m_state->clickedPointId.following);
                    seg.previous = segment.id;
                }
            }
            i++;
        }
    }

    // TODO check for reversion of start/end

    // We remove what should be removed. The indices are sorted given how we add them.
    // So we take them from last to first so that when removing in segments,
    // the order stays valid.
    for(auto st = indicesToRemove.rbegin(); st != indicesToRemove.rend(); ++st)
    {
        segments.removeAt(*st);
    }

    // Then we change the start/end of the correct segments
    setCurrentPoint(segments);
}


void MovePointCommandObject::handleCrossOnOverlap(QVector<CurveSegmentData>& segments)
{
    double current_x = m_state->currentPoint.x();
    // In this case we merge at the origins of the point and we create if it is in a new place.

    // First, if we go to the right.
    if(current_x > m_originalPress.x())
    {
        // Get the segment we're in, if there's any
        auto middleSegmentIt = std::find_if(segments.begin(), segments.end(),
                                            [&] (const CurveSegmentData& segment)
        {       // Going to the right
                return
                   segment.start.x() > m_originalPress.x()
                && segment.start.x() < current_x
                && segment.end.x() > current_x;
        });
        auto middle = middleSegmentIt != segments.end()
                ? &*middleSegmentIt
                : nullptr;

        // First part : removal of the segments around the initial click
        // If we have a following segment and the current position > end of the following segment
        if(m_state->clickedPointId.following)
        {
            auto foll_seg_it = find(segments, m_state->clickedPointId.following);
            auto& foll_seg = *foll_seg_it;
            if(current_x > foll_seg.end.x())
            {
                // If there was also a previous segment, it now goes to the end of the
                // presently removed segment.
                if(m_state->clickedPointId.previous)
                {
                    auto& prev_seg = *find(segments, m_state->clickedPointId.previous);
                    prev_seg.end = foll_seg.end;
                    prev_seg.following = foll_seg.following;
                }
                else
                {
                    // Link to the previous segment, or to the zero, maybe ?
                }

                // If the one about to be deleted also had a following, we set its previous
                // to the previous of the clicked point
                if(foll_seg.following)
                {
                    auto& foll_foll_seg = *find(segments, foll_seg.following);
                    foll_foll_seg.previous = m_state->clickedPointId.previous;
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
            for(auto& segment : segments)
            {
                auto seg_start_x = segment.start.x();
                if(seg_start_x < current_x && seg_start_x > m_originalPress.x())
                {
                    crossed = true;
                    break;
                }
            }

            if(crossed)
            {
                // We remove the previous of the clicked point
                auto prev_seg_it = find(segments, m_state->clickedPointId.previous);
                auto& prev_seg = *prev_seg_it;

                if(prev_seg.previous)
                {
                    // We set its following to null.
                    auto& prev_prev_seg = *find(segments, prev_seg.previous);
                    prev_prev_seg.following = Id<CurveSegmentModel>{};
                }

                segments.erase(prev_seg_it);
            }
            else
            {
                // We have not crossed a point
                setCurrentPoint(segments);
            }
        }

        // Second part : creation of a new segment where the cursor actually is
        if(middle)
        {
            // We insert a new element after the leftmost point from the current point.
            // Since we are in a segment we split it and create another with a new id.
            CurveSegmentData newSegment{
                        getSegmentId(segments),
                        middle->start,    m_state->currentPoint,
                        middle->previous, middle->id,
                        middle->type, middle->specificSegmentData
            };

            auto prev_it = find(segments, middle->previous);
            // TODO we shouldn't have to test for this, only test if middle->previous != id{}
            if(prev_it != segments.end())
            {
                (*prev_it).following = newSegment.id;
            }

            middle->start = m_state->currentPoint;
            middle->previous = newSegment.id;
            segments.push_back(newSegment);
        }
        else
        {
            /*
            // We're on the empty; we make a new linear segment from the end of the last point
            // or zero if there is none ? this can't happen unless we select a point on zero.

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
            auto newSegment = new LinearCurveSegmentModel{getStrongId(segments), nullptr};
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
    else if(current_x < m_originalPress.x())
    {
        // Get the segment we're in, if there's any
        auto middleSegmentIt = std::find_if(segments.begin(), segments.end(),
                                            [&] (const auto& segment)
        {       // Going to the left
                return
                   segment.end.x() < m_originalPress.x()
                && segment.start.x() < current_x
                && segment.end.x() > current_x;
        });
        auto middle = middleSegmentIt != segments.end()
                ? &*middleSegmentIt
                : nullptr;

        // First part : removal of the segments around the initial click
        // If we have a following segment and the current position > end of the following segment
        if(m_state->clickedPointId.previous)
        {
            auto prev_seg_it = find(segments, m_state->clickedPointId.previous);
            auto& prev_seg = *prev_seg_it;
            if(current_x < prev_seg.start.x())
            {
                // If there was also a following segment to the click, it now goes to the start of the
                // presently removed segment.
                if(m_state->clickedPointId.following)
                {
                    auto& foll_seg = *find(segments, m_state->clickedPointId.following);
                    foll_seg.start = prev_seg.start;
                    foll_seg.previous = prev_seg.previous;
                }
                else
                {
                    // Link to the following segment, or to the zero, maybe ?
                }

                // If the one about to be deleted also had a previous, we set its following
                // to the following of the clicked point
                if(prev_seg.previous)
                {
                    auto& prev_prev_seg = *find(segments, prev_seg.previous);
                    prev_prev_seg.following = m_state->clickedPointId.following;
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
            for(const auto& segment : segments)
            {
                auto seg_end_x = segment.end.x();
                if(seg_end_x > current_x && seg_end_x < m_originalPress.x())
                {
                    crossed = true;
                    break;
                }
            }

            if(crossed)
            {
                // We remove the following of the clicked point
                auto foll_seg_it = find(segments, m_state->clickedPointId.following);
                auto& foll_seg = *foll_seg_it;
                if(foll_seg.following)
                {
                    // We set its following to null.
                    auto& foll_foll_seg = *find(segments, foll_seg.following);
                    foll_foll_seg.previous = Id<CurveSegmentModel>{};
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
        if(middle)
        {

            CurveSegmentData newSegment{
                        getSegmentId(segments),
                        m_state->currentPoint, middle->end,
                        middle->id, middle->following,
                        middle->type, middle->specificSegmentData
            };

            auto foll_it = find(segments, middle->following);
            // TODO we shouldn't have to test for this, only test if middle->previous != id{}
            if(foll_it != segments.end())
            {
                (*foll_it).previous = newSegment.id;
            }

            middle->end = m_state->currentPoint;
            middle->following = newSegment.id;
            segments.push_back(newSegment);
        }
        else
        {
            // TODO (see the commented block on the symmetric part)
        }
    }
}

void MovePointCommandObject::setCurrentPoint(QVector<CurveSegmentData>& segments)
{
    CurveSegmentData* previousSegment = nullptr;
    CurveSegmentData* followingSegment = nullptr;
    // OPTIMIZEME /!\ quadratic /!\ also all these find_if
    for(auto& segment_sub : segments)
    {
        if(segment_sub.id == m_state->clickedPointId.previous)
            previousSegment = &segment_sub;
        if(segment_sub.id == m_state->clickedPointId.following)
            followingSegment = &segment_sub;

        if(previousSegment && followingSegment)
            break;
    }

    if(previousSegment)
    {
        previousSegment->end = m_state->currentPoint;
    }

    if(followingSegment)
    {
        followingSegment->start = m_state->currentPoint;
    }
}
