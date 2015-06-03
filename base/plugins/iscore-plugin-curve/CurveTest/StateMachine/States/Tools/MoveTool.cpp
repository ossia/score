#include "MoveTool.hpp"
#include "CurveTest/StateMachine/States/Move/MovePointState.hpp"
#include "CurveTest/StateMachine/States/Move/MoveSegmentState.hpp"
#include "CurveTest/CurveModel.hpp"
#include "CurveTest/OngoingCommandState.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include "CurveTest/MovePointCommandObject.hpp"
using namespace Curve;
MoveTool::MoveTool(CurveStateMachine& sm):
    CurveTool{sm, &sm}
{
    localSM().setObjectName("MoveToolStateMachine");
    m_waitState = new QState;
    localSM().addState(m_waitState);
    localSM().setInitialState(m_waitState);

    /// Point
    {
        auto mpco = new MovePointCommandObject(&sm.presenter(), sm.commandStack());
        auto movePoint = new OngoingState<Curve::Element::Point_tag>(*mpco, &localSM());
        movePoint->setObjectName("MovePointState");
        make_transition<ClickOnPoint_Transition>(m_waitState, movePoint, *movePoint);
        movePoint->addTransition(movePoint, SIGNAL(finished()), m_waitState);

        m_movePoint = movePoint;
        localSM().addState(m_movePoint);
    }

    /*
    /// Segment
    {
        auto msco = new MoveSegmentCommandObject(sm);
        auto moveSegment = new OngoingState<Curve::Element::Segment_tag>(*msco, &localSM());
        make_transition<ClickOnSegment_Transition>(m_waitState, moveSegment, *moveSegment);
        moveSegment->addTransition(moveSegment, SIGNAL(finished()), m_waitState);

        m_moveSegment = moveSegment;
        localSM().addState(m_moveSegment);
    }
    */
    localSM().start();
}


void MoveTool::on_pressed()
{
    qDebug() << m_parentSM.curvePoint << itemUnderMouse(m_parentSM.scenePoint)->type();
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
               [&] (const QGraphicsItem* point)
    {
        qDebug() << "ClickOnPoint_Event";
        localSM().postEvent(new ClickOnPoint_Event(m_parentSM.curvePoint, point));
    },
    [&] (const QGraphicsItem* segment)
    {
        qDebug() << "ClickOnSegment_Event";
        localSM().postEvent(new ClickOnSegment_Event(m_parentSM.curvePoint, segment));
    },
    [&] ()
    {
        qDebug() << Q_FUNC_INFO << 3;
    });
}

void MoveTool::on_moved()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
               [&] (const QGraphicsItem* point)
    {
        qDebug() << Q_FUNC_INFO << 1;
        localSM().postEvent(new MoveOnPoint_Event(m_parentSM.curvePoint, point));
    },
    [&] (const QGraphicsItem* segment)
    {
        qDebug() << Q_FUNC_INFO << 2;
        localSM().postEvent(new MoveOnSegment_Event(m_parentSM.curvePoint, segment));
    },
    [&] ()
    {
        qDebug() << Q_FUNC_INFO << 3;
        localSM().postEvent(new MoveOnNothing_Event(m_parentSM.curvePoint, nullptr));
    });
}

void MoveTool::on_released()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
               [&] (const QGraphicsItem* point)
    {
        qDebug() << Q_FUNC_INFO << 1;
        localSM().postEvent(new ReleaseOnPoint_Event(m_parentSM.curvePoint, point));
    },
    [&] (const QGraphicsItem* segment)
    {
        qDebug() << Q_FUNC_INFO << 2;
        localSM().postEvent(new ReleaseOnSegment_Event(m_parentSM.curvePoint, segment));
    },
    [&] ()
    {
        qDebug() << Q_FUNC_INFO << 3;
        //localSM().postEvent(new Move_Event(m_parentSM.scenePoint));
    });
}
