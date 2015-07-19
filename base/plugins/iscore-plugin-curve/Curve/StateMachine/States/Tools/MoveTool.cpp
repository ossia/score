#include "MoveTool.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/StateMachine/OngoingState.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QSignalTransition>
#include "Curve/StateMachine/CommandObjects/MovePointCommandObject.hpp"
#include "Curve/StateMachine/CommandObjects/CreatePointCommandObject.hpp"
#include "Curve/StateMachine/CommandObjects/SetSegmentParametersCommandObject.hpp"
using namespace Curve;
EditionTool::EditionTool(CurveStateMachine& sm):
    CurveTool{sm, &sm}
{
/*
    {
        m_createPointChoice = new QState{this};
        m_movePointChoice = new QState{this};
        m_setSegmentChoice = new QState{this};
        this->setInitialState(m_movePointChoice);

        auto t_create_move = new QSignalTransition(this, SIGNAL(setMoveState()), m_createPointChoice);
        t_create_move->setTargetState(m_movePointChoice);
        auto t_create_setSegment = new QSignalTransition(this, SIGNAL(setSetSegmentState()), m_createPointChoice);
        t_create_setSegment->setTargetState(m_setSegmentChoice);

        auto t_move_create = new QSignalTransition(this, SIGNAL(setCreationState()), m_movePointChoice);
        t_move_create->setTargetState(m_createPointChoice);
        auto t_move_setSegment = new QSignalTransition(this, SIGNAL(setSetSegmentState()), m_movePointChoice);
        t_move_setSegment->setTargetState(m_setSegmentChoice);

        auto t_setSegment_create = new QSignalTransition(this, SIGNAL(setCreationState()), m_setSegmentChoice);
        t_setSegment_create->setTargetState(m_createPointChoice);
        auto t_setSegment_move = new QSignalTransition(this, SIGNAL(setMoveState()), m_setSegmentChoice);
        t_setSegment_move->setTargetState(m_movePointChoice);

    }

    auto base = new QState;
    localSM().addState(base);
    localSM().setInitialState(base);
    localSM().start();
*/

}

/*
void EditionTool::changeMode(int state)
{
    emit exitState();
    switch(state)
    {
        case static_cast<int>(EditionTool::Mode::Create):
            emit setCreationState();
            break;
        case static_cast<int>(EditionTool::Mode::Move):
            emit setMoveState();
            break;
        case static_cast<int>(EditionTool::Mode::SetSegment):
            emit setSetSegmentState();
            break;
        default:
            Q_ASSERT(false);
            break;
    }
}

int EditionTool::mode() const
{
    if(m_createPointChoice->active())
        return (int)EditionTool::Mode::Create;
    if(m_movePointChoice->active())
        return (int)EditionTool::Mode::Move;
    if(m_setSegmentChoice->active())
        return (int)EditionTool::Mode::SetSegment;

    return -1;
}*/

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


MoveTool::MoveTool(CurveStateMachine &sm):
    EditionTool{sm}
{
    QState* waitState = new QState{&localSM()};

    auto co = new MovePointCommandObject(&sm.presenter(), sm.commandStack());
    auto state = new OngoingState(*co, &localSM());
    state->setObjectName("MovePointState");
    make_transition<ClickOnPoint_Transition>(waitState, state, *state);
    state->addTransition(state, SIGNAL(finished()), waitState);

    localSM().setInitialState(waitState);

    localSM().start();
}


SetSegmentTool::SetSegmentTool(CurveStateMachine &sm):
    EditionTool{sm}
{
    QState* waitState = new QState{&localSM()};

    auto co = new SetSegmentParametersCommandObject(&sm.presenter(), sm.commandStack());
    auto state = new OngoingState(*co, &localSM());
    state->setObjectName("SetSegmentParametersState");
    make_transition<ClickOnSegment_Transition>(waitState, state, *state);
    state->addTransition(state, SIGNAL(finished()), waitState);

    localSM().setInitialState(waitState);

    localSM().start();
}


CreateTool::CreateTool(CurveStateMachine &sm):
    EditionTool{sm}
{
    this->setObjectName("CreateTool");
    localSM().setObjectName("CreateToolLocalSM");
    QState* waitState = new QState{&localSM()};
    waitState->setObjectName("WaitState");

    auto co = new CreatePointCommandObject(&sm.presenter(), sm.commandStack());
    auto state = new OngoingState(*co, &localSM());
    state->setObjectName("CreatePointFromNothingState");
    make_transition<ClickOnSegment_Transition>(waitState, state, *state);
    make_transition<ClickOnNothing_Transition>(waitState, state, *state);
    state->addTransition(state, SIGNAL(finished()), waitState);

    localSM().setInitialState(waitState);

    localSM().start();
}
