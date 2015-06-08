#include "MoveTool.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/StateMachine/OngoingCommandState.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include "Curve/StateMachine/CommandObjects/MovePointCommandObject.hpp"
#include "Curve/StateMachine/CommandObjects/CreatePointCommandObject.hpp"
#include "Curve/StateMachine/CommandObjects/SetSegmentParametersCommandObject.hpp"
using namespace Curve;
EditionTool::EditionTool(CurveStateMachine& sm):
    CurveTool{sm, &sm}
{
    localSM().setObjectName("MoveToolStateMachine");
    m_waitState = new QState;
    localSM().addState(m_waitState);
    localSM().setInitialState(m_waitState);

    /// Move
    {
        auto mpco = new MovePointCommandObject(&sm.presenter(), sm.commandStack());
        auto movePoint = new OngoingState(*mpco, &localSM());
        movePoint->setObjectName("MovePointState");
        make_transition<ClickOnPoint_Transition>(m_waitState, movePoint, *movePoint);
        movePoint->addTransition(movePoint, SIGNAL(finished()), m_waitState);

        localSM().addState(movePoint);
    }

    /// Create
    /*
    {
        auto cpfnco = new CreatePointCommandObject(&sm.presenter(), sm.commandStack());
        auto createPointFromNothingState = new OngoingState(*cpfnco, &localSM());
        createPointFromNothingState->setObjectName("CreatePointFromNothingState");
        make_transition<ClickOnSegment_Transition>(m_waitState, createPointFromNothingState, *createPointFromNothingState);
        make_transition<ClickOnNothing_Transition>(m_waitState, createPointFromNothingState, *createPointFromNothingState);
        createPointFromNothingState->addTransition(createPointFromNothingState, SIGNAL(finished()), m_waitState);

        localSM().addState(createPointFromNothingState);
    }
    */

    /// Update
    {
        auto co = new SetSegmentParametersCommandObject(&sm.presenter(), sm.commandStack());
        auto state = new OngoingState(*co, &localSM());
        state->setObjectName("SetSegmentParametersState");
        make_transition<ClickOnSegment_Transition>(m_waitState, state, *state);
        //make_transition<ClickOnNothing_Transition>(m_waitState, createPointFromNothingState, *createPointFromNothingState);
        state->addTransition(state, SIGNAL(finished()), m_waitState);

        localSM().addState(state);
    }


    localSM().start();
}


void EditionTool::on_pressed()
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

void EditionTool::on_moved()
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

void EditionTool::on_released()
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
