#include "RemoveStateFromEvent.hpp"

#include "Document/Event/EventModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveStateFromStateModel::RemoveStateFromStateModel(ObjectPath &&statePath, const iscore::State& state):
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {std::move(statePath) },
    m_state{state}
{

}

void RemoveStateFromStateModel::undo()
{
    auto& st = m_path.find<StateModel>();
    st.addState(m_state);
}

void RemoveStateFromStateModel::redo()
{
    auto& st = m_path.find<StateModel>();
    st.removeState(m_state);
}

void RemoveStateFromStateModel::serializeImpl(QDataStream& s) const
{
    // TODO s << m_path << m_state;
}

void RemoveStateFromStateModel::deserializeImpl(QDataStream& s)
{
    // TODO s >> m_path >> m_state;
}
