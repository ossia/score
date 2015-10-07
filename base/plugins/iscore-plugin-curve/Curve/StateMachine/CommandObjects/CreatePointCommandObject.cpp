#include "CreatePointCommandObject.hpp"
#include "Curve/CurvePresenter.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"
#include "Curve/Point/CurvePointModel.hpp"
#include "Curve/Point/CurvePointView.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include "Curve/Segment/Linear/LinearCurveSegmentModel.hpp"
CreatePointCommandObject::CreatePointCommandObject(CurvePresenter *presenter, iscore::CommandStack &stack):
    CurveCommandObjectBase{presenter, stack}
{
}

void CreatePointCommandObject::on_press()
{
    // Save the start data.
    m_originalPress = m_state->currentPoint;

    for(CurvePointModel* pt : m_presenter->model().points())
    {
        auto pt_x = pt->pos().x();

        if(pt_x >= m_xmin && pt_x < m_originalPress.x())
        {
            m_xmin = pt_x;
        }
        if(pt_x <= m_xmax && pt_x > m_originalPress.x())
        {
            m_xmax = pt_x;
        }
    }

    move();
}

void CreatePointCommandObject::move()
{
    auto segments = m_startSegments;

    // Locking between bounds
    handleLocking();

    // Creation
    createPoint(segments);

    // Submit
    submit(std::move(segments));
}

void CreatePointCommandObject::release()
{
    m_dispatcher.commit();
}

void CreatePointCommandObject::cancel()
{
    m_dispatcher.rollback();
}

void CreatePointCommandObject::createPoint(std::vector<CurveSegmentData> &segments)
{
    // Create a point where we clicked
    // By creating segments that goes to the closest points if we're in the empty,
    // or by splitting a segment if we're in the middle of it.
    // 1. Check if we're clicking in a place where there is a segment
    CurveSegmentData* middle = nullptr;
    CurveSegmentData* exactBefore = nullptr;
    CurveSegmentData* exactAfter = nullptr;
    for(auto& segment : segments)
    {
        if(segment.start.x() < m_state->currentPoint.x() && m_state->currentPoint.x() < segment.end.x())
            middle = &segment;
        if(segment.end.x() == m_state->currentPoint.x())
            exactBefore = &segment;
        if(segment.start.x() == m_state->currentPoint.x())
            exactAfter = &segment;

        if(middle && exactBefore && exactAfter)
            break;
    }

    // Handle creation on an exact other point
    if(exactBefore || exactAfter)
    {
        if(exactBefore)
        {
            exactBefore->end = m_state->currentPoint;
        }
        if(exactAfter)
        {
            exactAfter->start = m_state->currentPoint;
        }
    }
    else if(middle)
    {
        // TODO refactor with MovePointState (line ~330)
        // The segment goes in the first half of "middle"
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
        // ~ Creating in the void ~ ... Spooky!

        // This is of *utmost* importance : if we don't do this,
        // when we push_back, the pointers get invalidated because the memory
        // has been moved, which causes a wealth of uncanny bugs and random memory errors
        // in other threads. So we reserve from up front the size we'll need.
        segments.reserve(segments.size() + 2);

        double seg_closest_from_left_x = 0;
        CurveSegmentData* seg_closest_from_left{};
        double seg_closest_from_right_x = 1.;
        CurveSegmentData* seg_closest_from_right{};
        for(CurveSegmentData& segment : segments)
        {
            auto seg_start_x = segment.start.x();
            if(seg_start_x > m_state->currentPoint.x() && seg_start_x < seg_closest_from_right_x)
            {
                seg_closest_from_right_x = seg_start_x;
                seg_closest_from_right = &segment;
            }

            auto seg_end_x = segment.end.x();
            if(seg_end_x < m_state->currentPoint.x() && seg_end_x > seg_closest_from_left_x)
            {
                seg_closest_from_left_x = seg_end_x;
                seg_closest_from_left = &segment;
            }
        }

        // Create a curve segment for the left
        // We do this little dance because of id generation.
        {
            CurveSegmentData newLeftSegment;
            newLeftSegment.id = getSegmentId(segments);
            segments.push_back(newLeftSegment);
        }
        CurveSegmentData& newLeftSegment = segments.back();
        newLeftSegment.type = "Linear";
        newLeftSegment.specificSegmentData = QVariant::fromValue(LinearCurveSegmentData{});
        newLeftSegment.start = {seg_closest_from_left_x, 0.};
        newLeftSegment.end = m_state->currentPoint;

        {
            CurveSegmentData newRightSegment;
            newRightSegment.id = getSegmentId(segments);
            segments.push_back(newRightSegment);
        }
        CurveSegmentData& newRightSegment = segments.back();
        newRightSegment.type = "Linear";
        newRightSegment.specificSegmentData = QVariant::fromValue(LinearCurveSegmentData{});
        newRightSegment.start = m_state->currentPoint;
        newRightSegment.end = {seg_closest_from_right_x, 0.};

        newLeftSegment.following = newRightSegment.id;
        newRightSegment.previous = newLeftSegment.id;

        if(seg_closest_from_left)
        {
            newLeftSegment.start = seg_closest_from_left->end;
            newLeftSegment.previous = seg_closest_from_left->id;

            seg_closest_from_left->following = newLeftSegment.id;
        }

        if(seg_closest_from_right)
        {
            newRightSegment.end = seg_closest_from_right->start;
            newRightSegment.following = seg_closest_from_right->id;

            seg_closest_from_right->previous = newRightSegment.id;
        }

    }
}
