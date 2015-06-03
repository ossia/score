#include "MovePointCommandObject.hpp"
#include "UpdateCurve.hpp"
#include "CurvePresenter.hpp"
#include "CurveModel.hpp"
#include "CurveSegmentModel.hpp"
#include "CurvePointModel.hpp"
#include "CurvePointView.hpp"
#include <iscore/document/DocumentInterface.hpp>

MovePointCommandObject::MovePointCommandObject(CurvePresenter* presenter, iscore::CommandStack& stack):
    CurveCommandObjectBase{presenter},
    m_dispatcher{stack}
{

}

void MovePointCommandObject::on_press()
{
    // Save the start data.
    auto currentPoint = *std::find_if(m_presenter->model()->points().begin(),
                                     m_presenter->model()->points().end(),
                                     [&] (CurvePointModel* pt)
    { return pt->previous()  == m_state->clickedPointId.previous
          && pt->following() == m_state->clickedPointId.following; });
    m_originalPress = currentPoint->pos();

    m_startSegments.clear();
    auto current = m_presenter->model()->segments();

    std::transform(current.begin(), current.end(), std::back_inserter(m_startSegments),
                   [] (CurveSegmentModel* segment)
    {
        QByteArray arr;
        Serializer<DataStream> s(&arr);
        s.readFrom(*segment);
        return arr;
    });


    // Compute xmin, xmax
    {
        xmin = 0;
        xmax = 1;
        // Look for the next and previous points

        for(CurvePointModel* pt : m_presenter->model()->points())
        {
            auto pt_x = pt->pos().x();
            if(pt == currentPoint)
                continue;

            if(pt_x >= xmin && pt_x < m_originalPress.x())
            {
                xmin = pt_x;
            }
            if(pt_x <= xmax && pt_x > m_originalPress.x())
            {
                xmax = pt_x;
            }
        }
    }
}

#include "CurveSegmentModelSerialization.hpp"
void MovePointCommandObject::move()
{
    // First we deserialize the base segments.
    QVector<CurveSegmentModel*> segments;
    std::transform(m_startSegments.begin(), m_startSegments.end(), std::back_inserter(segments),
                   [] (QByteArray arr)
    {
        Deserializer<DataStream> des(arr);
        return createCurveSegment(des, nullptr);
    });

    double current_x = m_state->currentPoint.x();
    // Manage locking
    if(m_presenter->lockBetweenPoints())
    {
        if(current_x < xmin)
            m_state->currentPoint.setX(xmin + 0.001);

        if(current_x > xmax)
            m_state->currentPoint.setX(xmax - 0.001);
    }

    // Manage point - segment replacement
    // In all cases, if we're going on the same position that any other point, this other point is removed and we replace it.
    for(CurveSegmentModel* segment : segments)
    {
        if(segment->start().x() == current_x)
        {
            segment->setStart(m_state->currentPoint);
        }

        if(segment->end().x() == current_x)
        {
            segment->setEnd(m_state->currentPoint);
        }
    }

    if(m_presenter->suppressOnOverlap())
    {
        // All segments contained between the starting position and current position are removed.
        // Only the starting segment perdures (or no segment if there was none.).

        auto segmentsCopy = segments;
        // First the case where we're going to the right.
        if(m_originalPress.x() < current_x)
        {
            for(CurveSegmentModel* segment : segmentsCopy)
            {
                auto seg_start_x = segment->start().x();
                auto seg_end_x = segment->end().x();

                if(seg_start_x >= m_originalPress.x()
                && seg_start_x < current_x
                && seg_end_x < current_x)
                {
                    // The segment is behind us, we delete it
                    delete segment;
                    segments.removeOne(segment);
                }
                else if(seg_start_x >= m_originalPress.x()
                     && seg_start_x < current_x)
                {
                    // We're on the middle of a segment
                    segment->setPrevious(m_state->clickedPointId.previous);
                    segment->setStart(m_state->currentPoint);
                    // If the new segment is non-sensical we remove it
                    if(segment->start().x() >= segment->end().x())
                    {
                        segments.removeOne(segment);
                        delete segment;
                    }
                    // The new "previous" segment becomes the previous segment of the moving point.
                    else if(m_state->clickedPointId.previous)
                    {
                        // We also set the following to the current segment if available.
                        auto it = std::find(segments.begin(), segments.end(), m_state->clickedPointId.previous);
                        (*it)->setFollowing(segment->id());
                    }
                }
            }
        }
        // Now the case where we're going to the left
        else if(m_originalPress.x() > current_x)
        {
            for(CurveSegmentModel* segment : segmentsCopy)
            {
                auto seg_start_x = segment->start().x();
                auto seg_end_x = segment->end().x();

                if(seg_end_x <= m_originalPress.x()
                && seg_start_x > current_x
                && seg_end_x > current_x)
                {
                    // If it had previous && next, they are merged
                    if(segment->previous() && segment->following())
                    {
                        CurveSegmentModel* seg_prev = *std::find(segments.begin(), segments.end(), segment->previous());
                        CurveSegmentModel* seg_foll = *std::find(segments.begin(), segments.end(), segment->following());

                        seg_prev->setFollowing(seg_foll->id());
                        seg_foll->setPrevious(seg_prev->id());
                    }
                    else if(segment->following())
                    {
                        CurveSegmentModel* seg_foll = *std::find(segments.begin(), segments.end(), segment->following());
                        seg_foll->setPrevious(id_type<CurveSegmentModel>{});
                    }
                    else if(segment->previous())
                    {
                        CurveSegmentModel* seg_prev = *std::find(segments.begin(), segments.end(), segment->previous());
                        seg_prev->setFollowing(id_type<CurveSegmentModel>{});
                    }

                    // The segment is in front of us, we delete it
                    delete segment;
                    segments.removeOne(segment);
                }
                else if(seg_end_x < m_originalPress.x()
                     && seg_end_x > current_x)
                {
                    segment->setFollowing(m_state->clickedPointId.following);
                    segment->setEnd(m_state->currentPoint);
                    if(m_state->clickedPointId.following)
                    {
                        // We also set the previous to the current segment if available.
                        auto seg = *std::find(segments.begin(), segments.end(), m_state->clickedPointId.following);
                        seg->setPrevious(segment->id());
                    }
                }
            }
        }

        // TODO check for reversion of start/end
    }
    else
    {
        // In this case we merge at the origins of the point and we create if it is in a new place.
    }

    // Then we change the start/end of the correct segments
    auto previousSegment = std::find(segments.begin(), segments.end(), m_state->clickedPointId.previous);
    auto followingSegment = std::find(segments.begin(), segments.end(), m_state->clickedPointId.following);

    if(previousSegment != segments.end())
    {
        (*previousSegment)->setEnd(m_state->currentPoint);
    }

    if(followingSegment != segments.end())
    {
        (*followingSegment)->setStart(m_state->currentPoint);
    }

    QVector<QByteArray> newSegments;
    std::transform(segments.begin(), segments.end(), std::back_inserter(newSegments),
                   [] (CurveSegmentModel* segment)
    {
        QByteArray arr;
        Serializer<DataStream> s(&arr);
        s.readFrom(*segment);
        return arr;
    });

    qDeleteAll(segments);

    m_dispatcher.submitCommand<UpdateCurve>(iscore::IDocument::path(m_presenter->model()),
                                            std::move(newSegments));
}

void MovePointCommandObject::release()
{
    m_dispatcher.commit();
}

void MovePointCommandObject::cancel()
{
    m_dispatcher.rollback();
}
