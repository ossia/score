#include "SmartTool.hpp"
#include <Curve/Palette/CurvePalette.hpp>

#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveView.hpp>

#include <Curve/Palette/OngoingState.hpp>
#include <Curve/Palette/CommandObjects/MovePointCommandObject.hpp>
#include <Curve/Palette/States/SelectionState.hpp>

#include <iscore/selection/SelectionStack.hpp>

#include <core/document/Document.hpp>

namespace Curve
{
SmartTool::SmartTool(Curve::ToolPalette& sm):
    CurveTool{sm}
{
    m_state = new Curve::SelectionState{
            iscore::IDocument::documentFromObject(m_parentSM.model())->selectionStack(),
            m_parentSM,
            m_parentSM.presenter().view(),
            &localSM()};

    localSM().setInitialState(m_state);

    {
        auto co = new MovePointCommandObject(&sm.presenter(), sm.commandStack());
        m_moveState = new Curve::OngoingState{*co, nullptr};

        m_moveState->setObjectName("MovePointState");

        iscore::make_transition<ClickOnPoint_Transition>(m_state,
                                                 m_moveState,
                                                 *m_moveState);

        m_moveState->addTransition(m_moveState,
                                  SIGNAL(finished()),
                                  m_state);

        localSM().addState(m_moveState);
    }

    localSM().start();
}

void SmartTool::on_pressed(QPointF scenePoint, Curve::Point curvePoint)
{
    mapTopItem(scenePoint, itemUnderMouse(scenePoint),
               [&] (const CurvePointView* point)
    {
        localSM().postEvent(new ClickOnPoint_Event(curvePoint, point));
        m_nothingPressed = false;
    },
    [&] (const CurveSegmentView* segment)
    {
        localSM().postEvent(new ClickOnSegment_Event(curvePoint, segment));
        m_nothingPressed = false;
    },
    [&] ()
    {
        localSM().postEvent(new iscore::Press_Event);
        m_nothingPressed = true;
    });
}

void SmartTool::on_moved(QPointF scenePoint, Curve::Point curvePoint)
{
    if (m_nothingPressed)
    {
        localSM().postEvent(new iscore::Move_Event);
    }
    else
    {
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
    }
}

void SmartTool::on_released(QPointF scenePoint, Curve::Point curvePoint)
{
    if(m_nothingPressed)
    {
        localSM().postEvent(new iscore::Release_Event); // select
        m_nothingPressed = false;

        return;
    }

    mapTopItem(scenePoint, itemUnderMouse(scenePoint),
               [&] (const CurvePointView* point)
    {
        m_state->dispatcher.setAndCommit(
                    filterSelections(&point->model(),
                                     m_parentSM.model().selectedChildren(),
                                     m_state->multiSelection()));


        localSM().postEvent(new ReleaseOnPoint_Event(curvePoint, point));
    },
    [&] (const CurveSegmentView* segment)
    {
        m_state->dispatcher.setAndCommit(
                    filterSelections(&segment->model(),
                                     m_parentSM.model().selectedChildren(),
                                     m_state->multiSelection()));

        localSM().postEvent(new ReleaseOnSegment_Event(curvePoint, segment));
    },
    [&] ()
    {
        localSM().postEvent(new ReleaseOnNothing_Event(curvePoint, nullptr));
    });
}


}
