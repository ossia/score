#include "MessageItemModelAlgorithms.hpp"
#include <ProcessInterface/State/MessageNode.hpp>

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
        const Id<Process>& proc)
{
    for(ProcessStateData& data : vec)
    {
        if(data.process == proc)
        {
            data.value = val;
            return;
        }
    }

    vec.push_back({proc, val});
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
        const Id<Process>& proc,
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
            { return data.process == proc; });

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
            { return data.process == proc; });

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
        const Id<Process>& proc,
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
                    updateNode(node.values.previousProcessValues, mess.value, proc);
                    break;
                }
                case Position::Following:
                {
                    updateNode(node.values.followingProcessValues, mess.value, proc);
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
        const Id<Process>& proc,
        Position pos)
{
    // If the message is in the tree, we add the process value.
    nodeInsertAction(node, msg, proc, pos);
    nodePruneAction(node, proc, pos);
}


void rec_updateTree(
        MessageNode& node,
        iscore::MessageList& lst,
        const Id<Process>& proc,
        Position pos)
{
    nodeAction(node, lst, proc, pos);

    for(auto& child : node)
    {
        rec_updateTree(child, lst, proc, pos);
    }
}

// TODO another one to refactor with merges
void merge_impl(
        MessageNode& base,
        const StateNodeMessage& message)
{
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
                node->values.userValue = message.values.userValue;
            }
        }
    }
}


void updateTreeWithMessageList(
        MessageNode& rootNode,
        iscore::MessageList lst)
{

    for(const auto& mess : lst)
    {
        merge_impl(
            rootNode,
            {mess.address, StateNodeValues{{}, {}, mess.value}});
    }
}


void updateTreeWithMessageList(
        MessageNode& rootNode,
        iscore::MessageList lst,
        const Id<Process>& proc,
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
        rec_updateTree(child, lst, proc, pos);
    }

    // Handle the remaining messages
    for(const auto& mess : lst)
    {
        merge_impl(
            rootNode,
            {mess.address, StateNodeValues{{}, {}, mess.value}});
    }
}
