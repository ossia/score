#pragma once
#include "ScenarioToolState.hpp"
#include <Scenario/Palette/ScenarioPaletteBaseStates.hpp>

#include <Scenario/Palette/Transitions/NothingTransitions.hpp>
#include <Scenario/Palette/Transitions/EventTransitions.hpp>
#include <Scenario/Palette/Transitions/StateTransitions.hpp>
#include <Scenario/Palette/Transitions/TimeNodeTransitions.hpp>

#include <Scenario/Palette/Tools/States/ScenarioCreation_FromEvent.hpp>
#include <Scenario/Palette/Tools/States/ScenarioCreation_FromState.hpp>
#include <Scenario/Palette/Tools/States/ScenarioCreation_FromNothing.hpp>
#include <Scenario/Palette/Tools/States/ScenarioCreation_FromTimeNode.hpp>

#include <Scenario/Palette/Tools/ScenarioToolState.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>
#include <Scenario/Document/TimeNode/TimeNodeView.hpp>

#include <iscore/statemachine/StateMachineTools.hpp>
#include <iscore/document/DocumentInterface.hpp>

class EventPresenter;
class TimeNodePresenter;
namespace Scenario
{

template<typename Scenario_T, typename ToolPalette_T>
class CreationTool final : public ToolBase<ToolPalette_T>
{
    public:
        CreationTool(ToolPalette_T& sm) :
            ToolBase<ToolPalette_T>{sm}
        {
            m_waitState = new QState;
            this->localSM().addState(m_waitState);
            this->localSM().setInitialState(m_waitState);

            Path<Scenario_T> scenarioPath = this->m_parentSM.model();

            //// Create from nothing ////
            m_createFromNothingState = new Creation_FromNothing<Scenario_T, ToolPalette_T>{
                    this->m_parentSM,
                    scenarioPath,
                    this->m_parentSM.context().commandStack, nullptr};

            iscore::make_transition<ClickOnNothing_Transition<Scenario_T>>(m_waitState, m_createFromNothingState, *m_createFromNothingState);
            m_createFromNothingState->addTransition(m_createFromNothingState, SIGNAL(finished()), m_waitState);

            this->localSM().addState(m_createFromNothingState);


            //// Create from an event ////
            m_createFromEventState = new Creation_FromEvent<Scenario_T, ToolPalette_T>{
                    this->m_parentSM,
                    scenarioPath,
                    this->m_parentSM.context().commandStack, nullptr};

            iscore::make_transition<ClickOnEvent_Transition<Scenario_T>>(m_waitState, m_createFromEventState, *m_createFromEventState);
            m_createFromEventState->addTransition(m_createFromEventState, SIGNAL(finished()), m_waitState);

            this->localSM().addState(m_createFromEventState);


            //// Create from a timenode ////
            m_createFromTimeNodeState = new Creation_FromTimeNode<Scenario_T, ToolPalette_T>{
                    this->m_parentSM,
                    scenarioPath,
                    this->m_parentSM.context().commandStack, nullptr};

            iscore::make_transition<ClickOnTimeNode_Transition<Scenario_T>>(m_waitState,
                                                                m_createFromTimeNodeState,
                                                                *m_createFromTimeNodeState);
            m_createFromTimeNodeState->addTransition(m_createFromTimeNodeState, SIGNAL(finished()), m_waitState);

            this->localSM().addState(m_createFromTimeNodeState);


            //// Create from a State ////
            m_createFromStateState = new Creation_FromState<Scenario_T, ToolPalette_T>{
                    this->m_parentSM,
                    scenarioPath,
                    this->m_parentSM.context().commandStack, nullptr};

            iscore::make_transition<ClickOnState_Transition<Scenario_T>>(m_waitState,
                                                             m_createFromStateState,
                                                             *m_createFromStateState);

            m_createFromStateState->addTransition(m_createFromStateState, SIGNAL(finished()), m_waitState);

            this->localSM().addState(m_createFromStateState);

            this->localSM().start();
        }

        void on_pressed(QPointF scene, Scenario::Point sp)
        {
            this->mapTopItem(this->itemUnderMouse(scene),

                       // Press a state
                       [&] (const Id<StateModel>& id)
            { this->localSM().postEvent(new ClickOnState_Event{id, sp}); },

            // Press an event
            [&] (const Id<EventModel>& id)
            { this->localSM().postEvent(new ClickOnEvent_Event{id, sp}); },

            // Press a TimeNode
            [&] (const Id<TimeNodeModel>& id)
            { this->localSM().postEvent(new ClickOnTimeNode_Event{id, sp}); },

            // Press a Constraint
            [&] (const Id<ConstraintModel>&)
            { },

            // Press a slot handle
            [&] (const SlotModel&)
            { },

            // Click on the background
            [&] ()
            {

                // Here we have the logic for the creation in nothing
                // where we instead choose the latest state if selected
                if(auto state = furthestSelectedState(this->m_parentSM.model()))
                {
                    if(this->m_parentSM.model().events.at(state->eventId()).date() < sp.date)
                    {
                        this->localSM().postEvent(new ClickOnState_Event{
                                                state->id(),
                                                sp});
                        return;
                    }
                }

                this->localSM().postEvent(new ClickOnNothing_Event{sp});

            });
        }
        void on_moved(QPointF scene, Scenario::Point sp)
        {
            if(auto cs = currentState())
            {
                mapWithCollision(scene,
                                 [&] (const Id<StateModel>& id)
                { this->localSM().postEvent(new MoveOnState_Event{id, sp}); },
                [&] (const Id<EventModel>& id)
                { this->localSM().postEvent(new MoveOnEvent_Event{id, sp}); },
                [&] (const Id<TimeNodeModel>& id)
                { this->localSM().postEvent(new MoveOnTimeNode_Event{id, sp}); },
                [&] ()
                { this->localSM().postEvent(new MoveOnNothing_Event{sp}); },
                cs->createdStates,
                cs->createdEvents,
                cs->createdTimeNodes);

            }
        }
        void on_released(QPointF scene, Scenario::Point sp)
        {
            if(auto cs = currentState())
            {
                mapWithCollision(scene,
                                 [&] (const Id<StateModel>& id)
                { this->localSM().postEvent(new ReleaseOnState_Event{id, sp}); },
                [&] (const Id<EventModel>& id)
                { this->localSM().postEvent(new ReleaseOnEvent_Event{id, sp}); },
                [&] (const Id<TimeNodeModel>& id)
                { this->localSM().postEvent(new ReleaseOnTimeNode_Event{id, sp}); },
                [&] ()
                { this->localSM().postEvent(new ReleaseOnNothing_Event{sp}); },
                cs->createdStates,
                cs->createdEvents,
                cs->createdTimeNodes);
            }
        }
    private:
        // Return the colliding elements that were not created in the current commands
        QList<Id<StateModel>> getCollidingStates(QPointF point, const QVector<Id<StateModel>>& createdStates)
        {
            return getCollidingModels(
                        this->m_parentSM.presenter().states(),
                        createdStates,
                        point);
        }
        QList<Id<EventModel>> getCollidingEvents(QPointF point, const QVector<Id<EventModel>>& createdEvents)
        {
            return getCollidingModels(
                        this->m_parentSM.presenter().events(),
                        createdEvents,
                        point);
        }
        QList<Id<TimeNodeModel>> getCollidingTimeNodes(QPointF point, const QVector<Id<TimeNodeModel>>& createdTimeNodes)
        {
            return getCollidingModels(
                        this->m_parentSM.presenter().timeNodes(),
                        createdTimeNodes,
                        point);
        }

        CreationState<Scenario_T, ToolPalette_T>* currentState() const
        {
            if(isStateActive(m_createFromEventState))
                return m_createFromEventState;
            else if(isStateActive(m_createFromNothingState))
                return m_createFromNothingState;
            else if(isStateActive(m_createFromStateState))
                return m_createFromStateState;
            else if(isStateActive(m_createFromTimeNodeState))
                return m_createFromTimeNodeState;
            else
                return nullptr;
        }

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

        Creation_FromNothing<Scenario_T, ToolPalette_T>* m_createFromNothingState{};
        Creation_FromEvent<Scenario_T, ToolPalette_T>* m_createFromEventState{};
        Creation_FromTimeNode<Scenario_T, ToolPalette_T>* m_createFromTimeNodeState{};
        Creation_FromState<Scenario_T, ToolPalette_T>* m_createFromStateState{};
        QState* m_waitState{};
};
}
