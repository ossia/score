#include "EditValue.hpp"
#include <Document/State/StateModel.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <ProcessInterface/State/ProcessStateDataInterface.hpp>


enum class Position {
    Previous, Following
};

bool match(MessageNode& node, const iscore::Message& mess)
{
    MessageNode* n = &node;

    auto path = toStringList(mess.address);
    std::reverse(path.begin(), path.end());
    int i = 0;
    int imax = path.size();
    while(n->parent() && i < imax)
    {
        if(n->name == path.at(i))
        {
            if(i == imax - 1 && !n->parent()->parent())
                return true;

            i++;
            n = n->parent();
            continue;
        }

        return false;
    }

    return false;
}

void updateNode(
        QVector<ProcessStateData>& vec,
        const iscore::Value& val,
        const Process* proc)
{
    for(ProcessStateData& data : vec)
    {
        if(data.process == proc)
        {
            data.value = val;
            return;
        }
    }

    vec.push_back({QPointer<const Process>(proc), val});
}

void rec_delete(MessageNode& node)
{
    if(node.childCount() == 0)
    {
        // kthxbai
        auto parent = node.parent();
        if(parent)
        {
            parent->removeChild(parent->begin() + parent->indexOfChild(&node));
            rec_delete(*parent);
        }
    }
}

void nodePruneAction(
        MessageNode& node,
        const Process* process,
        Position pos
        )
{
    // If there is no corresponding message in our list,
    // but there is a corresponding process in the tree,
    // we prune it
    bool deleteMe = false;
    switch(pos)
    {
        case Position::Previous:
        {
            bool deleteMeMaybe = std::any_of(node.values.previousProcessValues.begin(),
                                          node.values.previousProcessValues.end(),
                                          [&] (const ProcessStateData& data)
            { return data.process == process; });

            if(deleteMeMaybe
            && node.values.followingProcessValues.isEmpty()
            && !node.values.userValue)
            {
                deleteMe = true;
            }
            break;
        }
        case Position::Following:
        {
            bool deleteMeMaybe = std::any_of(node.values.followingProcessValues.begin(),
                                          node.values.followingProcessValues.end(),
                                          [&] (const ProcessStateData& data)
            { return data.process == process; });

            if(deleteMeMaybe
            && node.values.previousProcessValues.isEmpty()
            && !node.values.userValue)
            {
                deleteMe = true;
            }
            break;
        }
        default:
            ISCORE_ABORT;
            break;
    }

    if(deleteMe)
    {
        if(node.childCount() > 0)
        {
            // We just clear the data
            node.values = StateNodeValues{};
        }
        else
        {
            rec_delete(node);
        }
    }
}

void nodeInsertAction(
        MessageNode& node,
        iscore::MessageList& msg,
        const Process* process,
        Position pos
        )
{
    auto it = msg.begin();
    auto end = msg.end();
    while(it != end)
    {
        const auto& mess = *it;
        if(match(node, mess))
        {
            switch(pos)
            {
                case Position::Previous:
                {
                    updateNode(node.values.previousProcessValues, mess.value, process);
                    break;
                }
                case Position::Following:
                {
                    updateNode(node.values.followingProcessValues, mess.value, process);
                    break;
                }
                default:
                    ISCORE_ABORT;
                    break;
            }

            it = msg.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void nodeAction(
        MessageNode& node,
        iscore::MessageList& msg,
        const Process* process,
        Position pos)
{
    // If the message is in the tree, we add the process value.
    nodeInsertAction(node, msg, process, pos);
    nodePruneAction(node, process, pos);
}


void rec_updateTree(
        MessageNode& node,
        iscore::MessageList& lst,
        const Process* process,
        Position pos)
{
    nodeAction(node, lst, process, pos);

    for(auto& child : node)
    {
        rec_updateTree(child, lst, process, pos);
    }
}

// TODO another one to refactor with merges
void merge_impl(
        MessageNode& base,
        const StateNodeMessage& message)
{
    using iscore::Node;

    QStringList path = toStringList(message.addr);

    ptr<MessageNode> node = &base;
    for(int i = 0; i < path.size(); i++)
    {
        auto& children = node->children();
        auto it = std::find_if(
                    children.begin(), children.end(),
                    [&] (const auto& cur_node) {
            return cur_node.displayName() == path[i];
        });

        if(it == children.end())
        {
            // We have to start adding sub-nodes from here.
            ptr<MessageNode> parentnode{node};
            for(int k = i; k < path.size(); k++)
            {
                ptr<MessageNode> newNode;
                if(k < path.size() - 1)
                {
                    newNode = &parentnode->emplace_back(
                                StateNodeData{
                                    path[k],
                                    {}},
                                nullptr);
                }
                else
                {
                    newNode = &parentnode->emplace_back(
                                StateNodeData{
                                    path[k],
                                    message.values},
                                nullptr);
                }

                parentnode = newNode;
            }

            break;
        }
        else
        {
            node = &*it;

            if(i == path.size() - 1)
            {
                // We replace the value by the one in the message
                node->values = message.values;
            }
        }
    }
}

void updateTreeWithMessageList(
        MessageNode& rootNode,
        iscore::MessageList lst,
        Process* process,
        Position pos)
{
    // We go through the tree.
    // For each node :
    // If lst contains a message corresponding to the node, we add / update it (and remove it from lst).
    // If the node contains a message corresponding to the process but there is none in lst, we remove its process part
    // (and we remove it if it's empty (and we also remove all the parents that don't have a value recursively)

    // When the tree has been visited, if there are remaining addresses in lst, we insert them in the tree.

    // We don't perform anything on the root node
    for(auto& child : rootNode)
    {
        rec_updateTree(child, lst, process, pos);
    }

    // Handle the remaining messages
    for(const auto& mess : lst)
    {
        StateNodeMessage vals{mess.address, StateNodeValues{{}, {}, mess.value}};
    }
}

EditValue::EditValue(
        Path<MessageItemModel> &&device_tree,
        const MessageNodePath& nodePath,
        const iscore::Value& value):
    iscore::SerializableCommand{factoryName(),
                                commandName(),
                                description()},
    m_path{device_tree},
    m_nodePath{nodePath}
{
    auto& model = m_path.find();
    m_oldState = model.rootNode();

    auto n = m_nodePath.toNode(&model.rootNode());
    ISCORE_ASSERT(n && n->parent() && n->parent()->parent());

    auto& sm = model.stateModel;
    m_newState = m_oldState;
    iscore::MessageList mess{iscore::Message{address(*n), value}};

    // For all the nodes in m_newState :
    // - if there is a new message in lst, add it as "process" node in the tree
    // - if there is a message both in lst and in the tree, update it in the tree
    // - if there is a message in the tree with the process, but not in lst it means
    // that the process stopped using this address. Hence we remove the "process" part
    // of this node, and if there is no "user" or message from another process, we remove
    // the node altogether

    for(ProcessStateDataInterface* prevProc : sm.previousProcesses())
    {
        auto lst = prevProc->setMessages(mess, m_oldState);

        updateTreeWithMessageList(m_newState, lst, &prevProc->process(), Position::Previous);
    }
    for(ProcessStateDataInterface* prevProc : sm.followingProcesses())
    {
        auto lst = prevProc->setMessages(mess, m_oldState);

        updateTreeWithMessageList(m_newState, lst, &prevProc->process(), Position::Following);
    }

    // TODO one day there will also be State functions that will perform
    // some local computation.

}

void EditValue::undo()
{
    auto& model = m_path.find();
    model = m_oldState;
}

void EditValue::redo()
{
    auto& model = m_path.find();
    model = m_newState;
}

void EditValue::serializeImpl(QDataStream &d) const
{
    ISCORE_TODO;
    /*
    d << m_path
      << m_nodePath
      << m_old
      << m_new;
      */
}

void EditValue::deserializeImpl(QDataStream &d)
{
    /*
    d >> m_path
      >> m_nodePath
      >> m_old
      >> m_new;
      */
}
