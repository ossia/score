#pragma once
#include "ScenarioToolState.hpp"
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp>
class EventPresenter;
class TimeNodePresenter;
namespace Scenario
{
class CreationState;
class Creation_FromNothing;
class Creation_FromEvent;
class Creation_FromState;
class Creation_FromTimeNode;

class CreationTool final : public ToolBase
{
    public:
        CreationTool(ToolPalette& sm);

        void on_pressed(QPointF scene, Scenario::Point sp);
        void on_moved(QPointF scene, Scenario::Point sp);
        void on_released(QPointF scene, Scenario::Point sp);
    private:
        // Return the colliding elements that were not created in the current commands
        QList<Id<StateModel>> getCollidingStates(QPointF, const QVector<Id<StateModel>>& createdStates);
        QList<Id<EventModel>> getCollidingEvents(QPointF, const QVector<Id<EventModel>>& createdEvents);
        QList<Id<TimeNodeModel>> getCollidingTimeNodes(QPointF, const QVector<Id<TimeNodeModel>>& createdTimeNodes);

        Scenario::CreationState* currentState() const;

        template<typename StateFun,
                 typename EventFun,
                 typename TimeNodeFun,
                 typename NothingFun>
        void mapWithCollision(
                QPointF point,
                StateFun st_fun,
                EventFun ev_fun,
                TimeNodeFun tn_fun,
                NothingFun nothing_fun,
                const QVector<Id<StateModel>>& createdStates,
                const QVector<Id<EventModel>>& createdEvents,
                const QVector<Id<TimeNodeModel>>& createdTimeNodes)
        {
            auto collidingStates = getCollidingStates(point, createdStates);
            if(!collidingStates.empty())
            {
                st_fun(collidingStates.first());
                return;
            }

            auto collidingEvents = getCollidingEvents(point, createdEvents);
            if(!collidingEvents.empty())
            {
                ev_fun(collidingEvents.first());
                return;
            }

            auto collidingTimeNodes = getCollidingTimeNodes(point, createdTimeNodes);
            if(!collidingTimeNodes.empty())
            {
                tn_fun(collidingTimeNodes.first());
                return;
            }

            nothing_fun();
        }

        Creation_FromNothing* m_createFromNothingState{};
        Creation_FromEvent* m_createFromEventState{};
        Creation_FromTimeNode* m_createFromTimeNodeState{};
        Creation_FromState* m_createFromStateState{};
        QState* m_waitState{};
};
}
