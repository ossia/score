#include "CreationTool.hpp"

#include "Curve/CurveModel.hpp"
#include "Curve/StateMachine/OngoingCommandState.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include "Curve/StateMachine/States/Create/CreatePointFromNothingCommandObject.hpp"

using namespace Curve;
CreationTool::CreationTool(CurveStateMachine& sm):
    CurveTool{sm, &sm}
{
    localSM().setObjectName("CreationToolStateMachine");
    m_waitState = new QState;
    localSM().addState(m_waitState);
    localSM().setInitialState(m_waitState);

    /// Point
    {
        auto cpfnco = new CreatePointFromNothingCommandObject(&sm.presenter(), sm.commandStack());
        auto createPointFromNothingState = new OngoingState(*cpfnco, &localSM());
        createPointFromNothingState->setObjectName("CreatePointFromNothingState");
        make_transition<ClickOnAnything_Transition>(m_waitState, createPointFromNothingState, *createPointFromNothingState);
        createPointFromNothingState->addTransition(createPointFromNothingState, SIGNAL(finished()), m_waitState);

        m_createPointFromNothing = createPointFromNothingState;
        localSM().addState(m_createPointFromNothing);
    }

    localSM().start();
}


void CreationTool::on_pressed()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
               [&] (const QGraphicsItem* point)
    {
        localSM().postEvent(new ClickOnPoint_Event(m_parentSM.curvePoint, point));
    },
    [&] (const QGraphicsItem* segment)
    {
        localSM().postEvent(new ClickOnSegment_Event(m_parentSM.curvePoint, segment));
    },
    [&] ()
    {
        localSM().postEvent(new ClickOnNothing_Event(m_parentSM.curvePoint, nullptr));
    });
}

void CreationTool::on_moved()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
               [&] (const QGraphicsItem* point)
    {
        localSM().postEvent(new MoveOnPoint_Event(m_parentSM.curvePoint, point));
    },
    [&] (const QGraphicsItem* segment)
    {
        localSM().postEvent(new MoveOnSegment_Event(m_parentSM.curvePoint, segment));
    },
    [&] ()
    {
        localSM().postEvent(new MoveOnNothing_Event(m_parentSM.curvePoint, nullptr));
    });
}

void CreationTool::on_released()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
               [&] (const QGraphicsItem* point)
    {
        localSM().postEvent(new ReleaseOnPoint_Event(m_parentSM.curvePoint, point));
    },
    [&] (const QGraphicsItem* segment)
    {
        localSM().postEvent(new ReleaseOnSegment_Event(m_parentSM.curvePoint, segment));
    },
    [&] ()
    {
        localSM().postEvent(new ReleaseOnNothing_Event(m_parentSM.curvePoint, nullptr));
    });
}
