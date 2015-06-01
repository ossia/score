#pragma once
#include <QState>
#include <QGraphicsItem>
#include <QStateMachine>
#include <iscore/tools/SettableIdentifier.hpp>
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

template<typename PresenterArray, typename IdToIgnore>
auto getCollidingModels(const PresenterArray& array, const IdToIgnore& id, const QPointF& scenePoint)
{
    using namespace std;
    QList<id_type<typename std::decay<decltype(array[0]->model())>::type>> colliding;

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
class ScenarioToolState : public QState
{
    public:
        ScenarioToolState(const ScenarioStateMachine& sm);
        void start();

    protected:
        const id_type<EventModel>& itemToEventId(const QGraphicsItem*) const;
        const id_type<TimeNodeModel>& itemToTimeNodeId(const QGraphicsItem*) const;
        const id_type<ConstraintModel>& itemToConstraintId(const QGraphicsItem*) const;

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
            // Check if it is an event / timenode / constraint
            // Check if it is in our scenario.
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

                default:
                    nothing_fun();
                    break;
            }
        }

        QGraphicsItem* itemUnderMouse(const QPointF& point) const;

        virtual void on_scenarioPressed() = 0;
        virtual void on_scenarioMoved() = 0;
        virtual void on_scenarioReleased() = 0;

        QStateMachine m_localSM;
        const ScenarioStateMachine& m_sm;
        const QGraphicsScene& m_scene;
};
