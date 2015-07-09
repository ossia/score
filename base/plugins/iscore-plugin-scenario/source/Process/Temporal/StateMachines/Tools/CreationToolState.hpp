#pragma once
#include "ScenarioToolState.hpp"
#include "../ScenarioStateMachineBaseStates.hpp"
class EventPresenter;
class TimeNodePresenter;
class ScenarioCreation_FromNothing;
class ScenarioCreation_FromEvent;
class ScenarioCreation_FromState;
class ScenarioCreation_FromTimeNode;
class CreationToolState : public ScenarioTool
{
    public:
        CreationToolState(ScenarioStateMachine& sm);

        void on_pressed() override;
        void on_moved() override;
        void on_released() override;

    private:
        // Return the colliding elements that were not created in the current commands
        QList<id_type<StateModel>> getCollidingStates(const QVector<id_type<StateModel>>& createdStates);
        QList<id_type<EventModel>> getCollidingEvents(const QVector<id_type<EventModel>>& createdEvents);
        QList<id_type<TimeNodeModel>> getCollidingTimeNodes(const QVector<id_type<TimeNodeModel>>& createdTimeNodes);

        CreationState* currentState() const;

        template<typename StateFun,
                 typename EventFun,
                 typename TimeNodeFun,
                 typename NothingFun>
        void mapWithCollision(
                StateFun&& st_fun,
                EventFun&& ev_fun,
                TimeNodeFun&& tn_fun,
                NothingFun&& nothing_fun,
                const QVector<id_type<StateModel>>& createdStates,
                const QVector<id_type<EventModel>>& createdEvents,
                const QVector<id_type<TimeNodeModel>>& createdTimeNodes)
        {
            auto collidingStates = getCollidingStates(createdStates);
            if(!collidingStates.empty())
            {
                st_fun(collidingStates.first());
                return;
            }

            auto collidingEvents = getCollidingEvents(createdEvents);
            if(!collidingEvents.empty())
            {
                ev_fun(collidingEvents.first());
                return;
            }

            auto collidingTimeNodes = getCollidingTimeNodes(createdTimeNodes);
            if(!collidingTimeNodes.empty())
            {
                tn_fun(collidingTimeNodes.first());
                return;
            }

            nothing_fun();
        }

        ScenarioCreation_FromNothing* m_createFromNothingState{};
        ScenarioCreation_FromEvent* m_createFromEventState{};
        ScenarioCreation_FromTimeNode* m_createFromTimeNodeState{};
        ScenarioCreation_FromState* m_createFromStateState{};
        QState* m_waitState{};
};
