#include "SelectionTool.hpp"
#include <iscore/statemachine/CommonSelectionState.hpp>
#include "Curve/StateMachine/CurveStateMachine.hpp"

#include "Curve/CurveModel.hpp"
#include "Curve/CurvePresenter.hpp"
#include "Curve/CurveView.hpp"

#include "Curve/Point/CurvePointModel.hpp"
#include "Curve/Point/CurvePointView.hpp"

#include "Curve/Segment/CurveSegmentModel.hpp"
#include "Curve/Segment/CurveSegmentView.hpp"

#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/selection/SelectionStack.hpp>

#include <core/document/Document.hpp>

#include <QGraphicsScene>

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

            auto item = m_parentSM.scene().itemAt(m_parentSM.scenePoint, QTransform());
            if(!item)
                return;

            setSelection(item);
        }

        void on_moveAreaSelection() override
        {
            m_movePoint = m_parentSM.scenePoint;
            m_view.setSelectionArea(
                        QRectF{m_view.mapFromScene(m_initialPoint),
                               m_view.mapFromScene(m_movePoint)}.normalized());
            setSelectionArea(QRectF{m_initialPoint, m_movePoint}.normalized());
        }

        void on_releaseAreaSelection() override
        {
            m_view.setSelectionArea(QRectF{});
        }

        void on_deselect() override
        {
            dispatcher.setAndCommit(Selection{});
            m_view.setSelectionArea(QRectF{});
        }

        void on_delete() override
        {
        }

        void on_deleteContent() override
        {
        }

    private:
        void setSelection(QGraphicsItem* item)
        {
            Selection sel;
            switch(item->type())
            {
                case QGraphicsItem::UserType + 10:
                    sel = filterSelections(&static_cast<CurvePointView*>(item)->model(),
                                           m_parentSM.model().selectedChildren(),
                                           multiSelection());
                    break;
                case QGraphicsItem::UserType + 11:
                    sel = filterSelections(&static_cast<CurveSegmentView*>(item)->model(),
                                           m_parentSM.model().selectedChildren(),
                                           multiSelection());
                    break;
                default:
                    // deselect ?
                    break;
            }

            dispatcher.setAndCommit(sel);
        }

        void setSelectionArea(const QRectF& area)
        {
            using namespace std;
            QPainterPath path;
            path.addRect(area);
            Selection sel;

            auto items = m_parentSM.scene().items(path);

            for (const auto& item : items)
            {
                switch(item->type())
                {
                    case QGraphicsItem::UserType + 10:
                        sel.push_back(&static_cast<CurvePointView*>(item)->model());
                        break;
                    case QGraphicsItem::UserType + 11:
                        sel.push_back(&static_cast<CurveSegmentView*>(item)->model());
                        break;
                    default:
                        break;
                }
            }

            dispatcher.setAndCommit(filterSelections(sel,
                                                     m_parentSM.model().selectedChildren(),
                                                     multiSelection()));
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
    using namespace std;
    localSM().postEvent(new Press_Event);
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
