#include "SelectionToolState.hpp"
#include "Process/ScenarioGlobalCommandManager.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

#include "Process/Temporal/StateMachines/Transitions/ConstraintTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/EventTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/StateTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.hpp"

#include "States/MoveStates.hpp"

#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"

#include <iscore/document/DocumentInterface.hpp>

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/ConstraintView.hpp"
#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"

#include <core/document/Document.hpp>

#include <QGraphicsScene>

#include "States/ScenarioSelectionState.hpp"
#include "MoveSlotToolState.hpp"
#include "States/SlotStates.hpp"
#include "Process/Temporal/StateMachines/Transitions/SlotTransitions.hpp"

SelectionTool::SelectionTool(ScenarioStateMachine& sm):
    ScenarioTool{sm, &sm}
{
    m_state = new ScenarioSelectionState{
            iscore::IDocument::documentFromObject(m_parentSM.model())->selectionStack(),
            m_parentSM,
            m_parentSM.presenter().view(),
            &localSM()};

    localSM().setInitialState(m_state);

    /// Constraint
    /// //TODO remove useless arguments to ctor
    m_moveConstraint =
            new MoveConstraintState{
                  m_parentSM,
                  iscore::IDocument::safe_path(m_parentSM.model()),
                  m_parentSM.commandStack(),
                  m_parentSM.locker(),
                  nullptr};

    make_transition<ClickOnConstraint_Transition>(m_state,
                                                  m_moveConstraint,
                                                  *m_moveConstraint);
    m_moveConstraint->addTransition(m_moveConstraint,
                                    SIGNAL(finished()),
                                    m_state);
    localSM().addState(m_moveConstraint);


    /// Event
    m_moveEvent =
            new MoveEventState{
                  m_parentSM,
                  iscore::IDocument::safe_path(m_parentSM.model()),
                  m_parentSM.commandStack(),
                  m_parentSM.locker(),
                  nullptr};

    make_transition<ClickOnState_Transition>(m_state,
                                             m_moveEvent,
                                             *m_moveEvent);

    make_transition<ClickOnEvent_Transition>(m_state,
                                             m_moveEvent,
                                             *m_moveEvent);
    m_moveEvent->addTransition(m_moveEvent,
                               SIGNAL(finished()),
                               m_state);
    localSM().addState(m_moveEvent);


    /// TimeNode
    m_moveTimeNode =
            new MoveTimeNodeState{
                  m_parentSM,
                  iscore::IDocument::safe_path(m_parentSM.model()),
                  m_parentSM.commandStack(),
                  m_parentSM.locker(),
                  nullptr};

    make_transition<ClickOnTimeNode_Transition>(m_state,
                                             m_moveTimeNode,
                                             *m_moveTimeNode);
    m_moveTimeNode->addTransition(m_moveTimeNode,
                                  SIGNAL(finished()),
                                  m_state);
    localSM().addState(m_moveTimeNode);



    /// Slot resize
    auto resizeSlot = new ResizeSlotState{
            m_parentSM.commandStack(),
            m_parentSM,
            &localSM()};

    make_transition<ClickOnSlotHandle_Transition>(
                m_state,
                resizeSlot,
                *resizeSlot);

    resizeSlot->addTransition(resizeSlot,
                              SIGNAL(finished()),
                              m_state);
}


void SelectionTool::on_pressed()
{
    using namespace std;
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const id_type<StateModel>& id) // State
    {
        localSM().postEvent(new ClickOnState_Event{id, m_parentSM.scenarioPoint});
        m_nothingPressed = false;
    },
    [&] (const id_type<EventModel>& id) // Event
    {
        localSM().postEvent(new ClickOnEvent_Event{id, m_parentSM.scenarioPoint});
        m_nothingPressed = false;
    },
    [&] (const id_type<TimeNodeModel>& id) // TimeNode
    {
        localSM().postEvent(new ClickOnTimeNode_Event{id, m_parentSM.scenarioPoint});
        m_nothingPressed = false;
    },
    [&] (const id_type<ConstraintModel>& id) // Constraint
    {
        localSM().postEvent(new ClickOnConstraint_Event{id, m_parentSM.scenarioPoint});
        m_nothingPressed = false;
    },
    [&] (const SlotModel& slot) // Slot handle
    {
        localSM().postEvent(new ClickOnSlotHandle_Event{iscore::IDocument::safe_path(slot)});
        m_nothingPressed = true; // Because we use the Move_Event and Release_Event.
    },
    [&] ()
    {
        localSM().postEvent(new Press_Event);
        m_nothingPressed = true;
    });
}

void SelectionTool::on_moved()
{
    if (m_nothingPressed)
    {
        localSM().postEvent(new Move_Event);
    }
    else
    {
        mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
        [&] (const id_type<StateModel>& id)
        { localSM().postEvent(new MoveOnState_Event{id, m_parentSM.scenarioPoint}); },
        [&] (const id_type<EventModel>& id)
        { localSM().postEvent(new MoveOnEvent_Event{id, m_parentSM.scenarioPoint}); },
        [&] (const id_type<TimeNodeModel>& id)
        { localSM().postEvent(new MoveOnTimeNode_Event{id, m_parentSM.scenarioPoint}); },
        [&] (const id_type<ConstraintModel>& id)
        { localSM().postEvent(new MoveOnConstraint_Event{id, m_parentSM.scenarioPoint}); },
        [&] (const SlotModel& slot) // Slot handle
        { /* do nothing, we aren't in this part but in m_nothingPressed == true part */ },
        [&] ()
        { localSM().postEvent(new MoveOnNothing_Event{m_parentSM.scenarioPoint});});
    }
}

void SelectionTool::on_released()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const id_type<StateModel>& id) // State
    {
        const auto& elt = m_parentSM.presenter().states().at(id);

        m_state->dispatcher.setAndCommit(filterSelections(&elt.model(),
                                                   m_parentSM.model().selectedChildren(),
                                                   m_state->multiSelection()));

        localSM().postEvent(new ReleaseOnState_Event{id, m_parentSM.scenarioPoint});
    },
    [&] (const id_type<EventModel>& id) // Event
    {
        const auto& elt = m_parentSM.presenter().events().at(id);

        m_state->dispatcher.setAndCommit(filterSelections(&elt.model(),
                                                   m_parentSM.model().selectedChildren(),
                                                   m_state->multiSelection()));

        localSM().postEvent(new ReleaseOnEvent_Event{id, m_parentSM.scenarioPoint});
    },
    [&] (const id_type<TimeNodeModel>& id) // TimeNode
    {
        const auto& elt = m_parentSM.presenter().timeNodes().at(id);

        m_state->dispatcher.setAndCommit(filterSelections(&elt.model(),
                                                   m_parentSM.model().selectedChildren(),
                                                   m_state->multiSelection()));
        localSM().postEvent(new ReleaseOnTimeNode_Event{id, m_parentSM.scenarioPoint});
    },
    [&] (const id_type<ConstraintModel>& id) // Constraint
    {
        const auto& elt = m_parentSM.presenter().constraints().at(id);

        m_state->dispatcher.setAndCommit(filterSelections(&elt.model(),
                                                   m_parentSM.model().selectedChildren(),
                                                   m_state->multiSelection()));
        localSM().postEvent(new ReleaseOnConstraint_Event{id, m_parentSM.scenarioPoint});
    },
    [&] (const SlotModel& slot) // Slot handle
    {
        localSM().postEvent(new Release_Event); // select
        m_nothingPressed = false;
    },
    [&] ()
    {
        if(m_nothingPressed)
        {
            localSM().postEvent(new Release_Event); // select
            m_nothingPressed = false;
        }
        else
        {
            localSM().postEvent(new ReleaseOnNothing_Event{m_parentSM.scenarioPoint}); // end of move
        }
    } );

}


