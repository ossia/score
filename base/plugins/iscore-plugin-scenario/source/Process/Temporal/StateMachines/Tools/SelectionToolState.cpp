#include "SelectionToolState.hpp"
#include "Process/ScenarioGlobalCommandManager.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

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
#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"

#include <core/document/Document.hpp>

#include <QGraphicsScene>

#include <iscore/statemachine/CommonSelectionState.hpp>

#include "ScenarioSelectionState.hpp"

SelectionTool::SelectionTool(ScenarioStateMachine& sm):
    ScenarioTool{sm, &sm}
{
    m_state = new ScenarioSelectionState{
            iscore::IDocument::documentFromObject(m_parentSM.model())->selectionStack(),
            m_parentSM,
            m_parentSM.presenter().view(),
            &localSM()};

    localSM().setInitialState(m_state);
}


void SelectionTool::on_pressed()
{
    using namespace std;
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const id_type<EventModel>& id) // Event
    {

    },
    [&] (const id_type<TimeNodeModel>& id) // TimeNode
    {

    },
    [&] (const id_type<ConstraintModel>& id) // Constraint
    {

    },
    [&] () { localSM().postEvent(new Press_Event); });

}

void SelectionTool::on_moved()
{
    localSM().postEvent(new Move_Event);
    /*
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const id_type<EventModel>&) {  },
    [&] (const id_type<TimeNodeModel>&) { localSM().postEvent(new Move_Event); },
    [&] (const id_type<ConstraintModel>&) { localSM().postEvent(new Move_Event); },
    [&] () { localSM().postEvent(new Move_Event); });
    */
}

void SelectionTool::on_released()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const id_type<EventModel>& id) // Event
    {
        const auto& elt = m_parentSM.presenter().events().at(id);

        m_state->dispatcher.setAndCommit(filterSelections(&elt->model(),
                                                   m_parentSM.model().selectedChildren(),
                                                   m_state->multiSelection()));
    },
    [&] (const id_type<TimeNodeModel>& id) // TimeNode
    {
        const auto& elt = m_parentSM.presenter().timeNodes().at(id);

        m_state->dispatcher.setAndCommit(filterSelections(&elt->model(),
                                                   m_parentSM.model().selectedChildren(),
                                                   m_state->multiSelection()));
    },
    [&] (const id_type<ConstraintModel>& id) // Constraint
    {
        const auto& elt = m_parentSM.presenter().constraints().at(id);

        m_state->dispatcher.setAndCommit(filterSelections(&elt->model(),
                                                   m_parentSM.model().selectedChildren(),
                                                   m_state->multiSelection()));
    },
    [&] () { localSM().postEvent(new Release_Event); });

    /*
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const id_type<EventModel>&) {  },
    [&] (const id_type<TimeNodeModel>&) { localSM().postEvent(new Release_Event); },
    [&] (const id_type<ConstraintModel>&) { localSM().postEvent(new Release_Event); },
    [&] () { localSM().postEvent(new Release_Event); });
    */
}


