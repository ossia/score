// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SmartTool.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/Palette/CurvePalette.hpp>
#include <Curve/Palette/CurvePaletteBaseEvents.hpp>
#include <Curve/Palette/CurvePaletteBaseTransitions.hpp>
#include <Curve/Palette/OngoingState.hpp>
#include <Curve/Palette/States/SelectionState.hpp>
#include <Curve/Palette/Tools/CurveTool.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Point/CurvePointView.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/CurveSegmentView.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/statemachine/StateMachineUtils.hpp>
#include <score/tools/std/Optional.hpp>

namespace Curve
{
SmartTool::SmartTool(Curve::ToolPalette& sm, const score::DocumentContext& context)
    : CurveTool{sm}
    , m_co{sm.model(), &sm.presenter(), context.commandStack}
    , m_segmentCo{sm.model(), &sm.presenter(), context.commandStack}
{
  m_state = new Curve::SelectionState{
      context.selectionStack, m_parentSM, m_parentSM.presenter().view(), &localSM()};

  localSM().setInitialState(m_state);

  {
    m_moveState = new Curve::OngoingState{m_co, nullptr};

    m_moveState->setObjectName("MovePointState");

    score::make_transition<ClickOnPoint_Transition>(m_state, m_moveState, *m_moveState);

    m_moveState->addTransition(m_moveState, finishedState(), m_state);

    localSM().addState(m_moveState);
  }

  {
    m_moveSegmentState = new Curve::OngoingState{m_segmentCo, nullptr};
    m_moveSegmentState->setObjectName("MoveSegmentState");
    score::make_transition<ClickOnSegment_Transition>(
        m_state, m_moveSegmentState, *m_moveSegmentState);
    m_moveSegmentState->addTransition(m_moveSegmentState, finishedState(), m_state);
    localSM().addState(m_moveSegmentState);
  }

  localSM().start();
}

void SmartTool::on_pressed(QPointF scenePoint, Curve::Point curvePoint)
{
  m_dragged = false;
  m_segmentPressed = false;
  mapTopItem(
      scenePoint, itemUnderMouse(scenePoint),
      [&](const PointView* point) {
    localSM().postEvent(new ClickOnPoint_Event(curvePoint, point));
    m_nothingPressed = false;
      },
      [&](const SegmentView* segment) {
    localSM().postEvent(new ClickOnSegment_Event(curvePoint, segment));
    m_nothingPressed = false;
    m_segmentPressed = true;
  },
      [&]() {
    localSM().postEvent(new score::Press_Event);
    m_nothingPressed = true;
      });
}

void SmartTool::on_moved(QPointF scenePoint, Curve::Point curvePoint)
{
  if(m_nothingPressed)
  {
    localSM().postEvent(new score::Move_Event);
  }
  else
  {
    m_dragged = true;
    mapTopItem(
        scenePoint, itemUnderMouse(scenePoint),
        [&](const PointView* point) {
      localSM().postEvent(new MoveOnPoint_Event(curvePoint, point));
        },
        [&](const SegmentView* segment) {
      localSM().postEvent(new MoveOnSegment_Event(curvePoint, segment));
    },
        [&]() { localSM().postEvent(new MoveOnNothing_Event(curvePoint, nullptr)); });
  }
}

void SmartTool::on_released(QPointF scenePoint, Curve::Point curvePoint)
{
  if(m_nothingPressed)
  {
    localSM().postEvent(new score::Release_Event); // select
    m_nothingPressed = false;

    return;
  }

  mapTopItem(
      scenePoint, itemUnderMouse(scenePoint),
      [&](const PointView* point) {
    if(!(m_dragged && m_segmentPressed))
      select(point->model(), m_parentSM.model().selectedChildren());
    localSM().postEvent(new ReleaseOnPoint_Event(curvePoint, point));
      },
      [&](const SegmentView* segment) {
    // After a drag, the segments moved stay selected as they were
    if(!(m_dragged && m_segmentPressed))
      select(segment->model(), m_parentSM.model().selectedChildren());
    localSM().postEvent(new ReleaseOnSegment_Event(curvePoint, segment));
  },
      [&]() { localSM().postEvent(new ReleaseOnNothing_Event(curvePoint, nullptr)); });
}
}
