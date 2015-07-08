#include "AddStateToEvent.hpp"

#include "Document/State/DisplayedStateModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddStateToStateModel::AddStateToStateModel(ObjectPath&& statePath, const iscore::State &state) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {std::move(statePath) },
    m_state{state}
{

}

AddStateToStateModel::AddStateToStateModel(ObjectPath&& statePath, iscore::State &&state) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {std::move(statePath)},
    m_state{std::move(state)}
{

}

void AddStateToStateModel::undo()
{
    auto& state = m_path.find<StateModel>();
    state.removeState(m_state);
}

void AddStateToStateModel::redo()
{
    auto& state = m_path.find<StateModel>();
    state.addState(m_state);
}

void AddStateToStateModel::serializeImpl(QDataStream& s) const
{
    s << m_path << m_state;
}

void AddStateToStateModel::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_state;
}
