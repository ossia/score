#pragma once
#include "CurveTest/StateMachine/CurveStateMachine.hpp"

#include <iscore/statemachine/ToolState.hpp>
#include <QGraphicsItem>
class CurveStateMachine;
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
                PointFun&& pt_fun,
                SegmentFun&& seg_fun,
                NothingFun&& nothing_fun) const
        {
            if(!pressedItem)
            {
                nothing_fun();
                return;
            }

            // Additionnal check because here the items aren't defined by
            // their boundingRect() but their shape().
            if(pressedItem->contains(pressedItem->mapFromScene(m_parentSM.scenePoint)))
            {
                switch(pressedItem->type())
                {
                    case QGraphicsItem::UserType + 10:
                        pt_fun(pressedItem);
                        break;

                    case QGraphicsItem::UserType + 11:
                        seg_fun(pressedItem);
                        break;

                    default:
                        nothing_fun();
                        break;
                }
            }
            else
            {
                nothing_fun();
            }
        }

        const CurveStateMachine& m_parentSM;
};
