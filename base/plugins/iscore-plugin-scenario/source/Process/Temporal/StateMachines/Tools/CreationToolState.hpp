#pragma once
#include "ScenarioToolState.hpp"
#include "States/CreateEventState.hpp"

class EventPresenter;
class TimeNodePresenter;
class CreationToolState : public ScenarioToolState
{
    public:
        CreationToolState(const ScenarioStateMachine& sm);

        void on_scenarioPressed() override;
        void on_scenarioMoved() override;
        void on_scenarioReleased() override;

    private:
        QList<id_type<EventModel>> getCollidingEvents(const id_type<EventModel>& createdEvent);
        QList<id_type<TimeNodeModel>> getCollidingTimeNodes(const id_type<TimeNodeModel>& createdTimeNode);
        template<typename EventFun,
                 typename TimeNodeFun,
                 typename NothingFun>
        void mapWithCollision(
                EventFun&& ev_fun,
                TimeNodeFun&& tn_fun,
                NothingFun&& nothing_fun,
                const id_type<EventModel>& createdEvent,
                const id_type<TimeNodeModel>& createdTimeNode)
        {
            auto collidingEvents = getCollidingEvents(createdEvent);
            if(!collidingEvents.empty())
            {
                ev_fun(collidingEvents.first());
                return;
            }

            auto collidingTimeNodes = getCollidingTimeNodes(createdTimeNode);
            if(!collidingTimeNodes.empty())
            {
                tn_fun(collidingTimeNodes.first());
                return;
            }

            nothing_fun();
        }

        CreateFromEventState* m_createFromEventState{};
        CreateFromTimeNodeState* m_createFromTimeNodeState{};
        QState* m_waitState{};
};
