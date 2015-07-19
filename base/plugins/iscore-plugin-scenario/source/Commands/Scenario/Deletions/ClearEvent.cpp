#include "ClearEvent.hpp"

#include "Document/State/StateModel.hpp"

#include "Process/ScenarioModel.hpp"


using namespace iscore;
using namespace Scenario::Command;

ClearState::ClearState(ObjectPath&& path) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {std::move(path) }
{
    const auto& state = m_path.find<StateModel>();
    QDataStream s(&m_serializedStates, QIODevice::WriteOnly);
    s << state.states();
}

void ClearState::undo()
{
    auto& state = m_path.find<StateModel>();
    iscore::StateList states;
    QDataStream s(m_serializedStates);
    s >> states;

    state.replaceStates(states);
}

void ClearState::redo()
{
    auto& state = m_path.find<StateModel>();
    state.replaceStates({});
}

void ClearState::serializeImpl(QDataStream& s) const
{
    s << m_path << m_serializedStates;
}

void ClearState::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_serializedStates;
}
