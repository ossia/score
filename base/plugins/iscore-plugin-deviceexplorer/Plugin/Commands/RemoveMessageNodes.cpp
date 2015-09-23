#include "RemoveMessageNodes.hpp"





#include <iscore/serialization/VisitorCommon.hpp>

RemoveMessageNodes::RemoveMessageNodes(
        Path<iscore::MessageItemModel>&& device_tree,
        const iscore::NodeList& lst):
    iscore::SerializableCommand{factoryName(),
                                commandName(),
                                description()},
    m_path{device_tree}
{
    for(const auto& node : lst)
    {
        m_savedNodes.append(*node);
        m_nodePaths.append(*node);
    }
}

void RemoveMessageNodes::undo()
{
    auto& model = m_path.find();

    for(int i = 0; i < m_nodePaths.size(); i++)
    {
        auto path_copy = m_nodePaths[i];
        path_copy.removeLast();
        auto parent = path_copy.toNode(&model.rootNode());
        ISCORE_ASSERT(parent);

        model.insertNode(*parent, m_savedNodes[i], m_nodePaths[i].back());
    }
}

void RemoveMessageNodes::redo()
{
    auto& model = m_path.find();

    for(const auto& nodepath : m_nodePaths)
    {
        auto n = nodepath.toNode(&model.rootNode());
        ISCORE_ASSERT(n);
        ISCORE_ASSERT(n->parent());
        model.removeNode(n->parent()->iterOfChild(n));
    }
}

void RemoveMessageNodes::serializeImpl(QDataStream &d) const
{
    d << m_path
      << m_nodePaths
      << m_savedNodes;
}

void RemoveMessageNodes::deserializeImpl(QDataStream &d)
{
    d >> m_path
      >> m_nodePaths
      >> m_savedNodes;
}
