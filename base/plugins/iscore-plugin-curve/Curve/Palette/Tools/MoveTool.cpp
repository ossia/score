// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Curve/Palette/CurvePalette.hpp>
#include <QState>
#include <QStateMachine>
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/tools/std/Optional.hpp>

#include "MoveTool.hpp"
#include <Curve/Palette/CurvePaletteBaseEvents.hpp>
#include <Curve/Palette/CurvePaletteBaseTransitions.hpp>
#include <Curve/Palette/OngoingState.hpp>
#include <Curve/Palette/Tools/CurveTool.hpp>
#include <Curve/Point/CurvePointView.hpp>
#include <Curve/Segment/CurveSegmentView.hpp>

namespace Curve
{
EditionToolForCreate::EditionToolForCreate(Curve::ToolPalette& sm)
    : CurveTool{sm}
{
}

void EditionToolForCreate::on_pressed(
    QPointF scenePoint, Curve::Point curvePoint)
{
  mapTopItem(
      scenePoint, itemUnderMouse(scenePoint),
      [&](const PointView* point) {
        localSM().postEvent(new ClickOnPoint_Event(curvePoint, point));
      },
      [&](const SegmentView* segment) {
        localSM().postEvent(new ClickOnSegment_Event(curvePoint, segment));
      },
      [&]() {
        localSM().postEvent(new ClickOnNothing_Event(curvePoint, nullptr));
      });
}

void EditionToolForCreate::on_moved(
    QPointF scenePoint, Curve::Point curvePoint)
{
  mapTopItem(
      scenePoint, itemUnderMouse(scenePoint),
      [&](const PointView* point) {
        localSM().postEvent(new MoveOnPoint_Event(curvePoint, point));
      },
      [&](const SegmentView* segment) {
        localSM().postEvent(new MoveOnSegment_Event(curvePoint, segment));
      },
      [&]() {
        localSM().postEvent(new MoveOnNothing_Event(curvePoint, nullptr));
      });
}

void EditionToolForCreate::on_released(
    QPointF scenePoint, Curve::Point curvePoint)
{
  mapTopItem(
      scenePoint, itemUnderMouse(scenePoint),
      [&](const PointView* point) {
        localSM().postEvent(new ReleaseOnPoint_Event(curvePoint, point));
      },
      [&](const SegmentView* segment) {
        localSM().postEvent(new ReleaseOnSegment_Event(curvePoint, segment));
      },
      [&]() {
        localSM().postEvent(new ReleaseOnNothing_Event(curvePoint, nullptr));
      });
}

SetSegmentTool::SetSegmentTool(
    Curve::ToolPalette& sm, const iscore::DocumentContext& context)
    : EditionToolForCreate{sm}, m_co{sm.model(), context.commandStack}
{
  QState* waitState = new QState{&localSM()};

  auto state = new OngoingState(m_co, &localSM());
  state->setObjectName("SetSegmentParametersState");
  iscore::make_transition<ClickOnSegment_Transition>(waitState, state, *state);
  state->addTransition(state, finishedState(), waitState);

  localSM().setInitialState(waitState);

  localSM().start();
}

CreateTool::CreateTool(
    Curve::ToolPalette& sm, const iscore::DocumentContext& context)
    : EditionToolForCreate{sm}, m_co{&sm.presenter(), context.commandStack}
{
  localSM().setObjectName("CreateToolLocalSM");
  QState* waitState = new QState{&localSM()};
  waitState->setObjectName("WaitState");

  auto state = new OngoingState(m_co, &localSM());
  state->setObjectName("CreatePointFromNothingState");
  iscore::make_transition<ClickOnSegment_Transition>(waitState, state, *state);
  iscore::make_transition<ClickOnNothing_Transition>(waitState, state, *state);
  state->addTransition(state, finishedState(), waitState);

  localSM().setInitialState(waitState);

  localSM().start();
}

CreatePenTool::CreatePenTool(
    Curve::ToolPalette& sm, const iscore::DocumentContext& context)
    : EditionToolForCreate{sm}, m_co{&sm.presenter(), context.commandStack}
{
  localSM().setObjectName("CreatePenToolLocalSM");
  QState* waitState = new QState{&localSM()};
  waitState->setObjectName("WaitState");

  auto state = new OngoingState(m_co, &localSM());
  state->setObjectName("CreatePointFromNothingState");
  iscore::make_transition<ClickOnSegment_Transition>(waitState, state, *state);
  iscore::make_transition<ClickOnNothing_Transition>(waitState, state, *state);
  state->addTransition(state, finishedState(), waitState);

  localSM().setInitialState(waitState);

  localSM().start();
}
}
