#include "MoveTool.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/StateMachine/OngoingState.hpp"
#include "Curve/StateMachine/CommandObjects/MovePointCommandObject.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QSignalTransition>
#include "Curve/StateMachine/CommandObjects/CreatePointCommandObject.hpp"
#include "Curve/StateMachine/CommandObjects/SetSegmentParametersCommandObject.hpp"
#include <Curve/StateMachine/CurveStateMachine.hpp>

namespace Curve
{
EditionToolForCreate::EditionToolForCreate(Curve::ToolPalette& sm):
    CurveTool{sm}
{
}

void EditionToolForCreate::on_pressed(QPointF scenePoint, Curve::Point curvePoint)
{
    qDebug("1");
    m_prev = std::chrono::steady_clock::now();
    mapTopItem(scenePoint, itemUnderMouse(scenePoint),
               [&] (const CurvePointView* point)
    {
        localSM().postEvent(new ClickOnPoint_Event(curvePoint, point));
    },
    [&] (const CurveSegmentView* segment)
    {
        localSM().postEvent(new ClickOnSegment_Event(curvePoint, segment));
    },
    [&] ()
    {
        localSM().postEvent(new ClickOnNothing_Event(curvePoint, nullptr));
    });
}

void EditionToolForCreate::on_moved(QPointF scenePoint, Curve::Point curvePoint)
{
    qDebug("2");
    auto t = std::chrono::steady_clock::now();
    if(std::chrono::duration_cast<std::chrono::milliseconds>(t - m_prev).count() < 16)
    {
        // TODO here put a timer at 16 ms to trigger the last step if we stop moving.
        return;
    }

    mapTopItem(scenePoint, itemUnderMouse(scenePoint),
               [&] (const CurvePointView* point)
    {
        localSM().postEvent(new MoveOnPoint_Event(curvePoint, point));
    },
    [&] (const CurveSegmentView* segment)
    {
        localSM().postEvent(new MoveOnSegment_Event(curvePoint, segment));
    },
    [&] ()
    {
        localSM().postEvent(new MoveOnNothing_Event(curvePoint, nullptr));
    });

    m_prev = t;
}

void EditionToolForCreate::on_released(QPointF scenePoint, Curve::Point curvePoint)
{
    qDebug("3");
    mapTopItem(scenePoint, itemUnderMouse(scenePoint),
               [&] (const CurvePointView* point)
    {
        localSM().postEvent(new ReleaseOnPoint_Event(curvePoint, point));
    },
    [&] (const CurveSegmentView* segment)
    {
        localSM().postEvent(new ReleaseOnSegment_Event(curvePoint, segment));
    },
    [&] ()
    {
        localSM().postEvent(new ReleaseOnNothing_Event(curvePoint, nullptr));
    });
}


SetSegmentTool::SetSegmentTool(Curve::ToolPalette &sm):
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


CreateTool::CreateTool(Curve::ToolPalette &sm):
    EditionToolForCreate{sm}
{
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
