#pragma once
#include <QState>
#include <QGraphicsItem>
#include <QStateMachine>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/statemachine/ToolState.hpp>
class EventModel;
class TimeNodeModel;
class ConstraintModel;
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
auto getCollidingModels(const PresenterContainer& array, const IdToIgnore& id, const QPointF& scenePoint)
{
    using namespace std;
    QList<id_type<typename PresenterContainer::model_type>> colliding;

    for(const auto& elt : array)
    {
        if((!bool(id) || id != elt->id()) && isUnderMouse(elt->view(), scenePoint))
        {
            colliding.push_back(elt->model().id());
        }
    }
    // TODO sort the elements according to their Z pos.

    return colliding;
}

class ScenarioStateMachine;
class ScenarioTool : public ToolState
{
    public:
        ScenarioTool(const ScenarioStateMachine& sm, QState* parent);

    protected:
        const id_type<EventModel>& itemToEventId(const QGraphicsItem*) const;
        const id_type<TimeNodeModel>& itemToTimeNodeId(const QGraphicsItem*) const;
        const id_type<ConstraintModel>& itemToConstraintId(const QGraphicsItem*) const;
        const id_type<EventModel>& itemStateToEventId(const QGraphicsItem*) const;

        template<typename EventFun,
                 typename TimeNodeFun,
                 typename ConstraintFun,
                 typename NothingFun>
        void mapTopItem(
                const QGraphicsItem* pressedItem,
                EventFun&& ev_fun,
                TimeNodeFun&& tn_fun,
                ConstraintFun&& cst_fun,
                NothingFun&& nothing_fun) const
        {
            if(!pressedItem)
            {
                nothing_fun();
                return;
            }

            // Each time :
            // Check if it is an event / timenode / constraint.
            // If state, do the same like for event.
            // TODO Possible crash : Check if it is in our scenario.
            switch(pressedItem->type())
            {
                case QGraphicsItem::UserType + 1:
                    ev_fun(itemToEventId(pressedItem));
                    break;

                case QGraphicsItem::UserType + 2:
                    cst_fun(itemToConstraintId(pressedItem));
                    break;

                case QGraphicsItem::UserType + 3:
                    tn_fun(itemToTimeNodeId(pressedItem));
                    break;

                case QGraphicsItem::UserType + 4:
                    ev_fun(itemStateToEventId(pressedItem));
                    break;

                default:
                    nothing_fun();
                    break;
            }
        }

        const ScenarioStateMachine& m_parentSM;
};
