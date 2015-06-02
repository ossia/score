#pragma once
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

            // Each time :
            // Check if it is an event / timenode / constraint
            // Check if it is in our scenario.
            switch(pressedItem->type())
            {
                case QGraphicsItem::UserType + 1:
                    pt_fun(pressedItem);
                    break;

                case QGraphicsItem::UserType + 2:
                    seg_fun(pressedItem);
                    break;

                default:
                    nothing_fun();
                    break;
            }
        }

        const CurveStateMachine& m_parentSM;

};
