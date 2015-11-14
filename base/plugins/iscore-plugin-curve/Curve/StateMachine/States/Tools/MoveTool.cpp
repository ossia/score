#include "MoveTool.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/StateMachine/OngoingState.hpp"
#include "Curve/StateMachine/CommandObjects/MovePointCommandObject.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QSignalTransition>
#include "Curve/StateMachine/CommandObjects/CreatePointCommandObject.hpp"
#include "Curve/StateMachine/CommandObjects/SetSegmentParametersCommandObject.hpp"

namespace Curve
{





EditionToolForCreate::EditionToolForCreate(CurveStateMachine& sm):
    CurveTool{sm, &sm}
{
}

void EditionToolForCreate::on_pressed()
{
    m_prev = std::chrono::steady_clock::now();
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
               [&] (const CurvePointView* point)
    {
        localSM().postEvent(new ClickOnPoint_Event(m_parentSM.curvePoint, point));
    },
    [&] (const CurveSegmentView* segment)
    {
        localSM().postEvent(new ClickOnSegment_Event(m_parentSM.curvePoint, segment));
    },
    [&] ()
    {
        localSM().postEvent(new ClickOnNothing_Event(m_parentSM.curvePoint, nullptr));
    });
}

void EditionToolForCreate::on_moved()
{
    auto t = std::chrono::steady_clock::now();
    if(std::chrono::duration_cast<std::chrono::milliseconds>(t - m_prev).count() < 16)
    {
        return;
    }

    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
               [&] (const CurvePointView* point)
    {
        localSM().postEvent(new MoveOnPoint_Event(m_parentSM.curvePoint, point));
    },
    [&] (const CurveSegmentView* segment)
    {
        localSM().postEvent(new MoveOnSegment_Event(m_parentSM.curvePoint, segment));
    },
    [&] ()
    {
        localSM().postEvent(new MoveOnNothing_Event(m_parentSM.curvePoint, nullptr));
    });

    m_prev = t;
}

void EditionToolForCreate::on_released()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
               [&] (const CurvePointView* point)
    {
        localSM().postEvent(new ReleaseOnPoint_Event(m_parentSM.curvePoint, point));
    },
    [&] (const CurveSegmentView* segment)
    {
        localSM().postEvent(new ReleaseOnSegment_Event(m_parentSM.curvePoint, segment));
    },
    [&] ()
    {
        localSM().postEvent(new ReleaseOnNothing_Event(m_parentSM.curvePoint, nullptr));
    });
}


SetSegmentTool::SetSegmentTool(CurveStateMachine &sm):
    EditionToolForCreate{sm}
{
    QState* waitState = new QState{&localSM()};

    auto co = new SetSegmentParametersCommandObject(&sm.presenter(), sm.commandStack());
    auto state = new OngoingState(*co, &localSM());
    state->setObjectName("SetSegmentParametersState");
    iscore::make_transition<ClickOnSegment_Transition>(waitState, state, *state);
    state->addTransition(state, SIGNAL(finished()), waitState);

    localSM().setInitialState(waitState);

    localSM().start();
}


CreateTool::CreateTool(CurveStateMachine &sm):
    EditionToolForCreate{sm}
{
    this->setObjectName("CreateTool");
    localSM().setObjectName("CreateToolLocalSM");
    QState* waitState = new QState{&localSM()};
    waitState->setObjectName("WaitState");

    auto co = new CreatePointCommandObject(&sm.presenter(), sm.commandStack());
    auto state = new OngoingState(*co, &localSM());
    state->setObjectName("CreatePointFromNothingState");
    iscore::make_transition<ClickOnSegment_Transition>(waitState, state, *state);
    iscore::make_transition<ClickOnNothing_Transition>(waitState, state, *state);
    state->addTransition(state, SIGNAL(finished()), waitState);

    localSM().setInitialState(waitState);

    localSM().start();
}

}
