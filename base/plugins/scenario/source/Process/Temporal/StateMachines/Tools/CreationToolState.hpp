#pragma once
#include "GenericToolState.hpp"
#include "States/CreateEventState.hpp"

class CreationToolState : public GenericToolState
{
    public:
        CreationToolState(const ScenarioStateMachine& sm);

        void on_scenarioPressed() override;
        void on_scenarioMoved() override;
        void on_scenarioReleased() override;

    private:
        template<typename EventFun,
                 typename TimeNodeFun,
                 typename NothingFun>
        void mapWithCollision(
                EventFun&& ev_fun,
                TimeNodeFun&& tn_fun,
                NothingFun&& nothing_fun)
        {
            auto collidingEvents = getCollidingModels(m_sm.presenter().events(),
                                                      m_baseState->createdEvent());
            if(!collidingEvents.empty())
            {
                ev_fun(collidingEvents.first()->id());
                return;
            }

            auto collidingTimeNodes = getCollidingModels(m_sm.presenter().timeNodes(),
                                                         m_baseState->createdTimeNode());
            if(!collidingTimeNodes.empty())
            {
                tn_fun(collidingTimeNodes.first()->id());
                return;
            }

            nothing_fun();
        }

        CreateEventState* m_baseState{};
};
