#pragma once
#include <Scenario/Palette/Tools/ScenarioToolState.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>

#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>

#include <Scenario/Palette/Tools/States/ScenarioSelectionState.hpp>
#include <Scenario/Palette/Tools/States/ResizeSlotState.hpp>

#include <Scenario/Palette/Transitions/SlotTransitions.hpp>

namespace Scenario
{
class ToolPalette;

// TODO Generic smart tool with variadic...
template<
        typename Scenario_T,
        typename ToolPalette_T,
        typename View_T,
        typename MoveConstraintWrapper_T,
        typename MoveEventWrapper_T,
        typename MoveTimeNodeWrapper_T
        >
class SmartTool final : public ToolBase<ToolPalette_T>
{
    public:
        SmartTool(ToolPalette_T& sm):
            ToolBase<ToolPalette_T>{sm}
        {
            // Selection
            m_state = new SelectionState<ToolPalette_T, View_T>{
                      this->m_parentSM.context().selectionStack,
                      this->m_parentSM,
                      this->m_parentSM.presenter().view(),
                      &this->localSM()};

            this->localSM().setInitialState(m_state);

            // Other actions; they are in //.
            QState* actionsState = new QState(&this->localSM());
            {
                QState* waitState = new QState(actionsState);
                actionsState->setInitialState(waitState);

                MoveConstraintWrapper_T::template make<Scenario_T, ToolPalette_T>(this->m_parentSM, waitState, *actionsState);
                MoveEventWrapper_T::template make<Scenario_T, ToolPalette_T>(this->m_parentSM, waitState, *actionsState);
                MoveTimeNodeWrapper_T::template make<Scenario_T, ToolPalette_T>(this->m_parentSM, waitState, *actionsState);

                /// Slot resize
                auto resizeSlot = new ResizeSlotState<ToolPalette_T>{
                        this->m_parentSM.context().commandStack,
                        this->m_parentSM,
                        actionsState};

                make_transition<ClickOnSlotHandle_Transition>(
                            waitState,
                            resizeSlot,
                            *resizeSlot);

                resizeSlot->addTransition(resizeSlot,
                                          SIGNAL(finished()),
                                          waitState);
            }

            this->localSM().setChildMode(QState::ParallelStates);
            this->localSM().start();
        }

        void on_pressed(QPointF scene, Scenario::Point sp)
        {
            using namespace std;

            this->mapTopItem(this->itemUnderMouse(scene),
                       [&] (const Id<StateModel>& id) // State
            {
                this->localSM().postEvent(new ClickOnState_Event{id, sp});
                m_nothingPressed = false;
            },
            [&] (const Id<EventModel>& id) // Event
            {
                this->localSM().postEvent(new ClickOnEvent_Event{id, sp});
                m_nothingPressed = false;
            },
            [&] (const Id<TimeNodeModel>& id) // TimeNode
            {
                this->localSM().postEvent(new ClickOnTimeNode_Event{id, sp});
                m_nothingPressed = false;
            },
            [&] (const Id<ConstraintModel>& id) // Constraint
            {
                this->localSM().postEvent(new ClickOnConstraint_Event{id, sp});
                m_nothingPressed = false;
            },
            [&] (const SlotModel& slot) // Slot handle
            {
                this->localSM().postEvent(new ClickOnSlotHandle_Event{slot});
                m_nothingPressed = true; // Because we use the Move_Event and Release_Event.
            },
            [&] ()
            {
                this->localSM().postEvent(new Press_Event);
                m_nothingPressed = true;
            });
        }

        void on_moved(QPointF scene, Scenario::Point sp)
        {
            if (m_nothingPressed)
            {
                this->localSM().postEvent(new Move_Event);
            }
            else
            {
                this->mapTopItem(this->itemUnderMouse(scene),
                           [&] (const Id<StateModel>& id)
                { this->localSM().postEvent(new MoveOnState_Event{id, sp}); },
                [&] (const Id<EventModel>& id)
                { this->localSM().postEvent(new MoveOnEvent_Event{id, sp}); },
                [&] (const Id<TimeNodeModel>& id)
                { this->localSM().postEvent(new MoveOnTimeNode_Event{id, sp}); },
                [&] (const Id<ConstraintModel>& id)
                { this->localSM().postEvent(new MoveOnConstraint_Event{id, sp}); },
                [&] (const SlotModel& slot) // Slot handle
                { /* do nothing, we aren't in this part but in m_nothingPressed == true part */ },
                [&] ()
                { this->localSM().postEvent(new MoveOnNothing_Event{sp});});
            }
        }

        void on_released(QPointF scene, Scenario::Point sp)
        {
            if(m_nothingPressed)
            {
                this->localSM().postEvent(new Release_Event); // select
                m_nothingPressed = false;

                return;
            }

            this->mapTopItem(this->itemUnderMouse(scene),
                       [&] (const Id<StateModel>& id) // State
            {
                const auto& elt = this->m_parentSM.presenter().state(id);

                m_state->dispatcher.setAndCommit(filterSelections(&elt.model(),
                                                                  this->m_parentSM.model().selectedChildren(),
                                                                  m_state->multiSelection()));

                this->localSM().postEvent(new ReleaseOnState_Event{id, sp});
            },
            [&] (const Id<EventModel>& id) // Event
            {
                const auto& elt = this->m_parentSM.presenter().event(id);

                m_state->dispatcher.setAndCommit(filterSelections(&elt.model(),
                                                                  this->m_parentSM.model().selectedChildren(),
                                                                  m_state->multiSelection()));

                this->localSM().postEvent(new ReleaseOnEvent_Event{id, sp});
            },
            [&] (const Id<TimeNodeModel>& id) // TimeNode
            {
                const auto& elt = this->m_parentSM.presenter().timeNode(id);

                m_state->dispatcher.setAndCommit(filterSelections(&elt.model(),
                                                                  this->m_parentSM.model().selectedChildren(),
                                                                  m_state->multiSelection()));

                this->localSM().postEvent(new ReleaseOnTimeNode_Event{id, sp});
            },
            [&] (const Id<ConstraintModel>& id) // Constraint
            {
                const auto& elt = this->m_parentSM.presenter().constraint(id);

                m_state->dispatcher.setAndCommit(filterSelections(&elt.model(),
                                                                  this->m_parentSM.model().selectedChildren(),
                                                                  m_state->multiSelection()));

                this->localSM().postEvent(new ReleaseOnConstraint_Event{id, sp});
            },
            [&] (const SlotModel& slot) // Slot handle
            {
                this->localSM().postEvent(new Release_Event); // select
                m_nothingPressed = false; // TODO useless ???
            },
            [&] ()
            {
                this->localSM().postEvent(new ReleaseOnNothing_Event{sp}); // end of move
            } );

        }

    private:
        SelectionState<ToolPalette_T, View_T>* m_state{};

        bool m_nothingPressed{true};
};
}
