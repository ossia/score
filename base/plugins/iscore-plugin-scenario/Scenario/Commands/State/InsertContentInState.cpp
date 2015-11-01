#include "InsertContentInState.hpp"
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>


InsertContentInState::InsertContentInState(
        const QJsonObject& stateData,
        Path<StateModel>&& targetState):
    iscore::SerializableCommand{
        factoryName(),
        commandName(),
        description()},
    m_state{std::move(targetState)}
{
    // TODO ask what should be copied ? the state due to the processes ? the user state ?
    // For now we copy the whole value.
    // First recreate the tree

    // TODO we should update the processes here, and provide an API to do this
    // properly.

    auto& state = m_state.find();

    m_oldNode = state.messages().rootNode();
    m_newNode = m_oldNode;
    updateTreeWithMessageList(
                m_newNode,
                flatten(unmarshall<MessageNode>(stateData["Messages"].toObject()))
            );
}

void InsertContentInState::undo() const
{
    auto& state = m_state.find();
    state.messages() = m_oldNode;
}

void InsertContentInState::redo() const
{
    auto& state = m_state.find();
    state.messages() = m_newNode;
}

void InsertContentInState::serializeImpl(QDataStream& s) const
{
    s << m_oldNode << m_newNode << m_state;
}

void InsertContentInState::deserializeImpl(QDataStream& s)
{
    s >> m_oldNode >> m_newNode >> m_state;
}
