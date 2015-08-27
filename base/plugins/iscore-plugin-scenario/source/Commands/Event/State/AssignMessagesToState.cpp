#include "AssignMessagesToState.hpp"
using namespace iscore;
using namespace Scenario::Command;

AssignMessagesToState::AssignMessagesToState(
        Path<StateModel>&& path,
        StatePath&& statepath,
        const MessageList& messages):
    iscore::SerializableCommand{factoryName(), commandName(), description()},
    m_path{std::move(path)},
    m_statepath{std::move(statepath)},
    m_newMessages{std::move(messages)}
{
    auto& state = m_path.find();
    auto statenode = m_statepath.toNode(&state.states().rootNode());

    if(statenode->is<iscore::MessageList>())
    {
        m_oldMessages = statenode->get<iscore::MessageList>();
    }
    else
    {
        ISCORE_TODO;
    }
}


void AssignMessagesToState::undo()
{
    auto& state = m_path.find();

    state.states().setStateData(
                m_statepath.toNode(&state.states().rootNode()),
                m_oldMessages);
}

void AssignMessagesToState::redo()
{
    auto& state = m_path.find();
    state.states().setStateData(
                m_statepath.toNode(&state.states().rootNode()),
                m_newMessages);
}

void AssignMessagesToState::serializeImpl(QDataStream& s) const
{
    s << m_path << m_statepath << m_oldMessages << m_newMessages;
}

void AssignMessagesToState::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_statepath >> m_oldMessages >> m_newMessages;
}
