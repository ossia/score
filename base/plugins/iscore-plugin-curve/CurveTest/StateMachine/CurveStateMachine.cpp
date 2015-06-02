#include "CurveStateMachine.hpp"
#include "CurveTest/CurvePresenter.hpp"
#include "CurveTest/CurveView.hpp"

#include <iscore/statemachine/StateMachineUtils.hpp>

CurveStateMachine::CurveStateMachine(
        CurvePresenter& pres,
        QObject* parent):
    QStateMachine{parent},
    m_presenter{pres}
{
    auto QPointFToCurvePoint = [&] (const QPointF& point) -> CurvePoint
    {
        return {point.x() / m_presenter.view().boundingRect().width(),
                point.y() / m_presenter.view().boundingRect().height()};
    };

    auto updateData = [&] (const QPointF& point)
    {
        m_data.scenePoint = point;
        m_data.curvePoint = QPointFToCurvePoint(m_presenter.view().mapFromScene(point));
    };

    connect(&m_presenter.view(), &CurveView::pressed,
            this, [&] (const QPointF& point)
    {
        updateData(point);
        postEvent(new Press_Event);
    });

    connect(&m_presenter.view(), &CurveView::moved,
            this, [&] (const QPointF& point)
    {
        updateData(point);
        postEvent(new Move_Event);
    });

    connect(&m_presenter.view(), &CurveView::released,
            this, [&] (const QPointF& point)
    {
        updateData(point);
        postEvent(new Release_Event);
    });


    // TODO generalize this.
    connect(&m_presenter.view(), &CurveView::escPressed,
            this, [&] ()
    {
        this->postEvent(new Cancel_Event);
    });
}
