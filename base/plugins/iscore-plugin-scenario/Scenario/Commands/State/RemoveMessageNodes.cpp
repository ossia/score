#include "RemoveMessageNodes.hpp"
#include <iscore/serialization/VisitorCommon.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>

RemoveMessageNodes::RemoveMessageNodes(
        Path<MessageItemModel>&& device_tree,
        const QList<const MessageNode*>& nodes):
    m_path{device_tree}
{
    auto model = m_path.try_find();
    if(model)
    {
        m_oldState = model->rootNode();
    }

    m_newState = m_oldState;
    for(const auto& node : nodes)
    {
        updateTreeWithRemovedUserMessage(m_newState, message(*node));
    }
}

void RemoveMessageNodes::undo() const
{
    auto& model = m_path.find();
    model = m_oldState;
}

void RemoveMessageNodes::redo() const
{
    auto& model = m_path.find();
    model = m_newState;
}

void RemoveMessageNodes::serializeImpl(DataStreamInput &d) const
{
    d << m_path << m_oldState << m_newState;
}

void RemoveMessageNodes::deserializeImpl(DataStreamOutput &d)
{
    d >> m_path >> m_oldState >> m_newState;
}
