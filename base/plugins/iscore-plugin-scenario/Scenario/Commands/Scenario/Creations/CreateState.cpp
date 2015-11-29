#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <boost/core/explicit_operator_bool.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <algorithm>
#include <vector>

#include "CreateState.hpp"
#include "Scenario/Document/Event/EventModel.hpp"
#include "Scenario/Document/State/StateModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>

using namespace Scenario::Command;
CreateState::CreateState(const Scenario::ScenarioModel& scenario, const Id<EventModel> &event, double stateY) :
    m_path {scenario},
    m_newState{getStrongId(scenario.states)},
    m_event{event},
    m_stateY{stateY}
{

}

CreateState::CreateState(
        const Path<Scenario::ScenarioModel> &scenarioPath,
        const Id<EventModel> &event,
        double stateY):
    CreateState{scenarioPath.find(), event, stateY}
{

}

void CreateState::undo() const
{
    auto& scenar = m_path.find();

    ScenarioCreate<StateModel>::undo(
                m_newState,
                scenar);
    updateEventExtent(m_event, scenar);
}

void CreateState::redo() const
{
    auto& scenar = m_path.find();

    // Create the end state
    ScenarioCreate<StateModel>::redo(
                m_newState,
                scenar.events.at(m_event),
                m_stateY,
                scenar);

    updateEventExtent(m_event, scenar);
}


void CreateState::serializeImpl(DataStreamInput & s) const
{
    s << m_path << m_newState << m_event << m_stateY;
}


void CreateState::deserializeImpl(DataStreamOutput & s)
{
    s >> m_path >> m_newState >> m_event >> m_stateY;
}


