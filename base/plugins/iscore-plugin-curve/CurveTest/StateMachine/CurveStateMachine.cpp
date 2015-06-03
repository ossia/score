#include "CurveStateMachine.hpp"
#include "CurveTest/CurveModel.hpp"
#include "CurveTest/CurvePresenter.hpp"
#include "CurveTest/CurveView.hpp"
#include "CurveTest/StateMachine/States/Tools/SelectionTool.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <core/document/Document.hpp>
CurveStateMachine::CurveStateMachine(
        CurvePresenter& pres,
        QObject* parent):
    BaseStateMachine{*pres.view().scene()},
    m_presenter{pres},
    m_stack{
        iscore::IDocument::documentFromObject(
            m_presenter.model()) ->commandStack()
        },
    m_locker{
        iscore::IDocument::documentFromObject(
            m_presenter.model()) ->locker()
        }
{
    setupPostEvents();
    setupStates();

    start();
}

const CurvePresenter& CurveStateMachine::presenter() const
{
    return m_presenter;
}

const CurveModel& CurveStateMachine::model() const
{
    return *m_presenter.model();
}

iscore::CommandStack& CurveStateMachine::commandStack() const
{
    return m_stack;
}

iscore::ObjectLocker& CurveStateMachine::locker() const
{
    return m_locker;
}

void CurveStateMachine::setupStates()
{
    m_selectTool = new Curve::SelectionTool(*this);
    this->setInitialState(m_selectTool);
}

void CurveStateMachine::setupPostEvents()
{
    auto QPointFToCurvePoint = [&] (const QPointF& point) -> CurvePoint
    {
        return {point.x() / m_presenter.view().boundingRect().width(),
                point.y() / m_presenter.view().boundingRect().height()};
    };

    auto updateData = [=] (const QPointF& point)
    {
        scenePoint = point;
        curvePoint = QPointFToCurvePoint(m_presenter.view().mapFromScene(point));
    };

    connect(&m_presenter.view(), &CurveView::pressed,
            this, [=] (const QPointF& point)
    {
        updateData(point);
        postEvent(new Press_Event);
    });

    connect(&m_presenter.view(), &CurveView::moved,
            this, [=] (const QPointF& point)
    {
        updateData(point);
        postEvent(new Move_Event);
    });

    connect(&m_presenter.view(), &CurveView::released,
            this, [=] (const QPointF& point)
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

