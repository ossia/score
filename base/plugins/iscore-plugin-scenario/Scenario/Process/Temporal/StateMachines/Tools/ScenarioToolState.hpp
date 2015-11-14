#pragma once
#include <QState>
#include <QGraphicsItem>
#include <QStateMachine>
#include <chrono>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/statemachine/ToolState.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioPoint.hpp>
class EventModel;
class TimeNodeModel;
class ConstraintModel;
class StateModel;
class SlotModel;

namespace iscore
{
    class SerializableCommand;
}

template<typename Element>
bool isUnderMouse(Element ev, const QPointF& scenePos)
{
    return ev->mapRectToScene(ev->boundingRect()).contains(scenePos);
}

template<typename PresenterContainer, typename IdToIgnore>
QList<Id<typename PresenterContainer::model_type>>
    getCollidingModels(const PresenterContainer& array, const QVector<IdToIgnore>& ids, QPointF scenePoint)
{
    using namespace std;
    QList<Id<typename PresenterContainer::model_type>> colliding;

    for(const auto& elt : array)
    {
        if(!ids.contains(elt.id()) && isUnderMouse(elt.view(), scenePoint))
        {
            colliding.push_back(elt.model().id());
        }
    }
    // TODO sort the elements according to their Z pos.

    return colliding;
}
namespace Scenario
{
class ToolPalette;
class ToolBase : public GraphicsSceneToolBase<Scenario::Point>
{
    public:
        ToolBase(const Scenario::ToolPalette& sm);

    protected:
        Id<EventModel> itemToEventId(const QGraphicsItem*) const;
        Id<TimeNodeModel> itemToTimeNodeId(const QGraphicsItem*) const;
        Id<ConstraintModel> itemToConstraintId(const QGraphicsItem*) const;
        Id<StateModel> itemToStateId(const QGraphicsItem*) const;
        const SlotModel* itemToSlotFromHandle(const QGraphicsItem *pressedItem) const;

        template<typename EventFun,
                 typename StateFun,
                 typename TimeNodeFun,
                 typename ConstraintFun,
                 typename SlotHandleFun,
                 typename NothingFun>
        void mapTopItem(
                const QGraphicsItem* item,
                StateFun st_fun,
                EventFun ev_fun,
                TimeNodeFun tn_fun,
                ConstraintFun cst_fun,
                SlotHandleFun handle_fun,
                NothingFun nothing_fun) const
        {
            if(!item)
            {
                nothing_fun();
                return;
            }
            auto tryFun = [=] (auto fun, const auto& id)
            {
                if(id) fun(id);
                else   nothing_fun();
            };

            // Each time :
            // Check if it is an event / timenode / constraint /state
            // The itemToXXXId methods check that we are in the correct scenario, too.
            switch(item->type())
            {
                case QGraphicsItem::UserType + 1:
                    tryFun(ev_fun, itemToEventId(item));
                    break;

                case QGraphicsItem::UserType + 2:
                    tryFun(cst_fun, itemToConstraintId(item));
                    break;

                case QGraphicsItem::UserType + 3:
                    tryFun(tn_fun, itemToTimeNodeId(item));
                    break;

                case QGraphicsItem::UserType + 4:
                    tryFun (st_fun, itemToStateId(item));
                    break;

                case QGraphicsItem::UserType + 5: // Slot handle
                {
                    auto slot = itemToSlotFromHandle(item);
                    if(slot)
                    {
                        handle_fun(*slot);
                    }
                    else
                    {
                        nothing_fun();
                    }
                    break;
                }

                case QGraphicsItem::UserType + 6: // Constraint header
                {
                    tryFun(cst_fun, itemToConstraintId(item->parentItem()));
                    break;
                }
                default:
                    nothing_fun();
                    break;
            }
        }

        const Scenario::ToolPalette& m_parentSM;

        std::chrono::steady_clock::time_point m_prev;
};
}
