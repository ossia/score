#pragma once
#include "Curve/StateMachine/CurveStateMachine.hpp"
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/CurveSegmentView.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Point/CurvePointView.hpp>

#include <iscore/statemachine/ToolState.hpp>
#include <QGraphicsItem>
class CurveStateMachine;
namespace Curve
{
class CurveTool : public ToolState
{
    public:
        CurveTool(const CurveStateMachine&, QState* parent);

    protected:
        template<typename PointFun,
                 typename SegmentFun,
                 typename NothingFun>
        void mapTopItem(
                const QGraphicsItem* pressedItem,
                PointFun pt_fun,
                SegmentFun seg_fun,
                NothingFun nothing_fun) const
        {
            if(!pressedItem)
            {
                nothing_fun();
                return;
            }

            switch(pressedItem->type())
            {
                case QGraphicsItem::UserType + 10:
                {
                    auto pt = safe_cast<const CurvePointView*>(pressedItem);
                    if(pt->contains(pt->mapFromScene(m_parentSM.scenePoint)))
                        pt_fun(pt);
                    break;
                }

                case QGraphicsItem::UserType + 11:
                {
                    auto segt = safe_cast<const CurveSegmentView*>(pressedItem);
                    if(segt->contains(segt->mapFromScene(m_parentSM.scenePoint)))
                    {
                        seg_fun(segt);
                    }
                    break;
                }

                default:
                {
                    nothing_fun();
                    break;
                }
            }
        }

        const CurveStateMachine& m_parentSM;
};



class EditionToolForCreate : public CurveTool
{
        Q_OBJECT
    public:
        explicit EditionToolForCreate(CurveStateMachine& sm);

    protected:
        void on_pressed() final override;
        void on_moved() final override;
        void on_released() final override;

    private:
        std::chrono::steady_clock::time_point m_prev;
};
}
