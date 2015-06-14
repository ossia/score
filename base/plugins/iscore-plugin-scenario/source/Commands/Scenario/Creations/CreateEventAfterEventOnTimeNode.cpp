#include "CreateEventAfterEventOnTimeNode.hpp"

#include "source/Process/ScenarioModel.hpp"
#include "source/Document/Event/EventModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/TimeNode/TimeNodeModel.hpp"
#include "source/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
using namespace iscore;
using namespace Scenario::Command;

CreateEventAfterEventOnTimeNode::CreateEventAfterEventOnTimeNode(ObjectPath&& scenarioPath,
                                                                 id_type<EventModel> sourceevent,
                                                                 id_type<TimeNodeModel> timenode,
                                                                 const TimeValue& date,
                                                                 double height):
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {std::move(scenarioPath) },
    m_timeNodeId {timenode},
    m_firstEventId {sourceevent},
    m_time {date},
    m_heightPosition {height}
{
    auto& scenar = m_path.find<ScenarioModel>();

    m_createdEventId = getStrongId(scenar.events());
    m_createdConstraintId = getStrongId(scenar.constraints());

    // For each ScenarioViewModel of the scenario we are applying this command in,
    // we have to generate ConstraintViewModels, too
    for(auto& viewModel : viewModels(scenar))
    {
        m_createdConstraintViewModelIDs[iscore::IDocument::path(viewModel)] = getStrongId(viewModel->constraints());
    }

    // Finally, the id of the full view
    m_createdConstraintFullViewId = getStrongId(m_createdConstraintViewModelIDs.values().toVector());
}

void CreateEventAfterEventOnTimeNode::undo()
{
    auto& scenar = m_path.find<ScenarioModel>();
    StandardRemovalPolicy::removeEventAndConstraints(scenar, m_createdEventId);
}

void CreateEventAfterEventOnTimeNode::redo()
{
    auto& scenar = m_path.find<ScenarioModel>();

    CreateConstraintAndEvent(m_createdConstraintId,
                             m_createdConstraintFullViewId,
                             scenar.event(m_firstEventId),
                             scenar.timeNode(m_timeNodeId),
                             m_createdEventId,
                             m_time,
                             m_heightPosition,
                             scenar);

    // Creation of all the constraint view models
    createConstraintViewModels(m_createdConstraintViewModelIDs,
                               m_createdConstraintId,
                               scenar);
}



void CreateEventAfterEventOnTimeNode::serializeImpl(QDataStream& s) const
{
    s << m_path
      << m_firstEventId
      << m_time
      << m_heightPosition
      << m_createdEventId
      << m_createdConstraintId
      << m_timeNodeId
      << m_createdConstraintViewModelIDs
      << m_createdConstraintFullViewId;
}

void CreateEventAfterEventOnTimeNode::deserializeImpl(QDataStream& s)
{
    s >> m_path
            >> m_firstEventId
            >> m_time
            >> m_heightPosition
            >> m_createdEventId
            >> m_createdConstraintId
            >> m_timeNodeId
            >> m_createdConstraintViewModelIDs
            >> m_createdConstraintFullViewId;
}
