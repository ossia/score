#include "ClearEvent.hpp"

#include "Document/State/StateModel.hpp"

#include "Process/ScenarioModel.hpp"

#include <iscore/serialization/VisitorCommon.hpp>


using namespace iscore;
using namespace Scenario::Command;

ClearState::ClearState(ModelPath<StateModel>&& path) :
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_path {std::move(path) }
{
    const auto& state = m_path.find();

    m_serializedStates = marshall<DataStream>(state.states().rootNode());
}

void ClearState::undo()
{
    auto& state = m_path.find();
    iscore::StateNode states;
    QDataStream s(m_serializedStates);
    s >> states;

    state.states() = states;
}

void ClearState::redo()
{
    auto& state = m_path.find();

    state.states() = iscore::StateNode{};
}

void ClearState::serializeImpl(QDataStream& s) const
{
    s << m_path << m_serializedStates;
}

void ClearState::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_serializedStates;
}
