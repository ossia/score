#include "CreatePointCommandObject.hpp"
#include "Curve/CurvePresenter.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"
#include "Curve/Point/CurvePointModel.hpp"
#include "Curve/Point/CurvePointView.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include "Curve/Segment/LinearCurveSegmentModel.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>
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
    auto segments = deserializeSegments();

    // Locking between bounds
    handleLocking();

    // Creation
    createPoint(segments);

    // Submit
    submit(segments);
}

void CreatePointCommandObject::release()
{
    m_dispatcher.commit();
}

void CreatePointCommandObject::cancel()
{
    m_dispatcher.rollback();
}

void CreatePointCommandObject::createPoint(QVector<CurveSegmentModel *> &segments)
{
    // Create a point where we clicked
    // By creating segments that goes to the closest points if we're in the empty,
    // or by splitting a segment if we're in the middle of it.
    // 1. Check if we're clicking in a place where there is a segment
    CurveSegmentModel* middle = nullptr;
    CurveSegmentModel* exactBefore = nullptr;
    CurveSegmentModel* exactAfter = nullptr;
    for(CurveSegmentModel* segment : segments)
    {
        if(segment->start().x() < m_state->currentPoint.x() && m_state->currentPoint.x() < segment->end().x())
        {
            middle = segment;
        }
        if(segment->end().x() == m_state->currentPoint.x())
        {
            exactBefore = segment;
        }
        if(segment->start().x() == m_state->currentPoint.x())
        {
            exactAfter= segment;
        }
    }

    // Handle creation on an exact other point
    if(exactBefore || exactAfter)
    {
        if(exactBefore)
        {
            exactBefore->setEnd(m_state->currentPoint);
        }
        if(exactAfter)
        {
            exactAfter->setStart(m_state->currentPoint);
        }
    }
    else if(middle)
    {
        // TODO refactor with MovePointState
        // The segment goes in the first half of "middle"
        auto newSegment = middle->clone(getStrongId(segments), nullptr);
        newSegment->setStart(middle->start());
        newSegment->setEnd(m_state->currentPoint);
        newSegment->setPrevious(middle->previous());
        newSegment->setFollowing(middle->id());

        auto prev_it = std::find(segments.begin(), segments.end(), middle->previous());
        if(prev_it != segments.end())
        {
            (*prev_it)->setFollowing(newSegment->id());
        }

        middle->setStart(m_state->currentPoint);
        middle->setPrevious(newSegment->id());
        segments.append(newSegment);
    }
    else
    {
        double seg_closest_from_left_x = 0;
        CurveSegmentModel* seg_closest_from_left{};
        double seg_closest_from_right_x = 1.;
        CurveSegmentModel* seg_closest_from_right{};
        for(CurveSegmentModel* segment : segments)
        {
            auto seg_start_x = segment->start().x();
            if(seg_start_x > m_state->currentPoint.x() && seg_start_x < seg_closest_from_right_x)
            {
                seg_closest_from_right_x = seg_start_x;
                seg_closest_from_right = segment;
            }

            auto seg_end_x = segment->end().x();
            if(seg_end_x < m_state->currentPoint.x() && seg_end_x > seg_closest_from_left_x)
            {
                seg_closest_from_left_x = seg_end_x;
                seg_closest_from_left = segment;
            }
        }

        // Create a curve segment for the left
        auto newLeftSegment = new LinearCurveSegmentModel{getStrongId(segments), nullptr};
        segments.append(newLeftSegment);
        newLeftSegment->setStart({seg_closest_from_left_x, 0.});
        newLeftSegment->setEnd(m_state->currentPoint);

        auto newRightSegment = new LinearCurveSegmentModel{getStrongId(segments), nullptr};
        segments.append(newRightSegment);
        newRightSegment->setStart(m_state->currentPoint);
        newRightSegment->setEnd({seg_closest_from_right_x, 0.});

        newLeftSegment->setFollowing(newRightSegment->id());
        newRightSegment->setPrevious(newLeftSegment->id());

        if(seg_closest_from_left)
        {
            newLeftSegment->setStart(seg_closest_from_left->end());
            newLeftSegment->setPrevious(seg_closest_from_left->id());

            seg_closest_from_left->setFollowing(newLeftSegment->id());
        }

        if(seg_closest_from_right)
        {
            newRightSegment->setEnd(seg_closest_from_right->start());
            newRightSegment->setFollowing(seg_closest_from_right->id());

            seg_closest_from_right->setPrevious(newRightSegment->id());
        }
    }
}
