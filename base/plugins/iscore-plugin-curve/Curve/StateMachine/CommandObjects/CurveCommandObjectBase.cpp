#include "CurveCommandObjectBase.hpp"
#include "Curve/CurvePresenter.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"
#include "Curve/Point/CurvePointModel.hpp"
#include "Curve/Segment/CurveSegmentModelSerialization.hpp"

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/document/DocumentInterface.hpp>

CurveCommandObjectBase::CurveCommandObjectBase(
        CurvePresenter* pres,
        iscore::CommandStack& stack):
    m_presenter{pres},
    m_dispatcher{stack}
{

}

void CurveCommandObjectBase::press()
{
    const auto& current = m_presenter->model();

    // Serialize the current state of the curve
    m_startSegments = current.toCurveData();

    // To prevent behind locked at 0.000001 or 0.9999
    m_xmin = -1;
    m_xmax = 2;

    on_press();
}

void CurveCommandObjectBase::handleLocking()
{
    double current_x = m_state->currentPoint.x();
    double current_y = m_state->currentPoint.y();

    // In any case we lock between O - 1 in both axes.
    if(current_x < 0.)
        m_state->currentPoint.setX(0.);
    if(current_x > 1.)
        m_state->currentPoint.setX(1.);
    if(current_y < 0.)
        m_state->currentPoint.setY(0.);
    if(current_y > 1.)
        m_state->currentPoint.setY(1.);

    // And more specifically...
    if(m_presenter->lockBetweenPoints())
    {
        if(current_x <= m_xmin)
            m_state->currentPoint.setX(m_xmin + 0.000001);

        if(current_x >= m_xmax)
            m_state->currentPoint.setX(m_xmax - 0.000001);
    }
}

QVector<CurveSegmentModel*> CurveCommandObjectBase::deserializeSegments() const
{
    QVector<CurveSegmentModel*> segments;
    segments.reserve(m_startSegments.size());
    std::transform(m_startSegments.begin(), m_startSegments.end(), std::back_inserter(segments),
                   [] (const CurveSegmentData& arr)
    {
        return createCurveSegment(arr, nullptr);
    });

    return segments;
}

void CurveCommandObjectBase::submit(const QVector<CurveSegmentData>& segments)
{
    // TODO std::move
    m_dispatcher.submitCommand(m_presenter->model(),
                               segments);
}
