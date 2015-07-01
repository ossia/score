#pragma once
#include <QState>
#include <QGraphicsItem>
#include <QStateMachine>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/statemachine/ToolState.hpp>
class EventModel;
class TimeNodeModel;
class ConstraintModel;
class DisplayedStateModel;

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
        id_type<EventModel> itemToEventId(const QGraphicsItem*) const;
        id_type<TimeNodeModel> itemToTimeNodeId(const QGraphicsItem*) const;
        id_type<ConstraintModel> itemToConstraintId(const QGraphicsItem*) const;
        id_type<DisplayedStateModel> itemToStateId(const QGraphicsItem*) const;

        template<typename EventFun,
                 typename StateFun,
                 typename TimeNodeFun,
                 typename ConstraintFun,
                 typename NothingFun>
        void mapTopItem(
                const QGraphicsItem* item,
                EventFun&& ev_fun,
                StateFun&& st_fun,
                TimeNodeFun&& tn_fun,
                ConstraintFun&& cst_fun,
                NothingFun&& nothing_fun) const
        {
            if(!item)
            {
                nothing_fun();
                return;
            }
            auto&& tryFun = [&] (auto&& fun, auto&& id)
            { if(id)
                fun(id);
              else
                nothing_fun();
            };

            // Each time :
            // Check if it is an event / timenode / constraint /state
            // TODO Possible crash : Check if it is in our scenario. -> done in itemToEltId
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

                default:
                    nothing_fun();
                    break;
            }
        }

        const ScenarioStateMachine& m_parentSM;
};
