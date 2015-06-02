#include "SelectionTool.hpp"
#include <iscore/statemachine/CommonSelectionState.hpp>
#include "CurveTest/StateMachine/CurveStateMachine.hpp"

#include "CurveTest/CurveModel.hpp"
#include "CurveTest/CurvePresenter.hpp"
#include "CurveTest/CurveView.hpp"
#include "CurveTest/CurvePointView.hpp"
#include "CurveTest/CurveSegmentView.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <iscore/selection/SelectionStack.hpp>

namespace Curve
{
class SelectionState : public CommonSelectionState
{
    private:
        QPointF m_initialPoint;
        QPointF m_movePoint;

        const CurveStateMachine& m_parentSM;
        CurveView& m_view;


    public:
        SelectionState(
                iscore::SelectionStack& stack,
                const CurveStateMachine& parentSM,
                CurveView& view,
                QState* parent):
            CommonSelectionState{stack, &view, parent},
            m_parentSM{parentSM},
            m_view{view}
        {
        }

        const QPointF& initialPoint() const
        { return m_initialPoint; }
        const QPointF& movePoint() const
        { return m_movePoint; }

        void on_pressAreaSelection() override
        {
            m_initialPoint = m_parentSM.scenePoint;
        }

        void on_moveAreaSelection() override
        {
            m_movePoint = m_parentSM.scenePoint;
            m_view.setSelectionArea(
                        QRectF{m_view.mapFromScene(m_initialPoint),
                               m_view.mapFromScene(m_movePoint)}.normalized());

        }

        void on_releaseAreaSelection() override
        {
            m_view.setSelectionArea(QRectF{});
        }

        void on_deselect() override
        {
            m_view.setSelectionArea(QRectF{});
        }

        void on_delete() override
        {
        }

        void on_deleteContent() override
        {
        }
};

SelectionTool::SelectionTool(CurveStateMachine& sm):
    CurveTool{sm, &sm}
{

    m_state = new Curve::SelectionState{
            iscore::IDocument::documentFromObject(m_parentSM.model())->selectionStack(),
            m_parentSM,
            m_parentSM.presenter().view(),
            &localSM()};

    localSM().setInitialState(m_state);

    localSM().start();
}


void SelectionTool::on_pressed()
{
    qDebug() << Q_FUNC_INFO;
    using namespace std;
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const QGraphicsItem* point)
    {
        localSM().postEvent(new Press_Event);
    },
    [&] (const QGraphicsItem* segment)
    {
        localSM().postEvent(new Press_Event);
    },
    [&] () {
        localSM().postEvent(new Press_Event);
    });

}

void SelectionTool::on_moved()
{
    localSM().postEvent(new Move_Event);
}

void SelectionTool::on_released()
{
    localSM().postEvent(new Release_Event);
}

}
