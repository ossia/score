#include <Device/Node/DeviceNode.hpp>
#include <Process/State/MessageNode.hpp>
#include <boost/optional/optional.hpp>
#include <QtGlobal>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTypeInfo>
#include <QVector>
#include <algorithm>
#include <vector>
#include <set>

#include "MessageItemModelAlgorithms.hpp"
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/std/Algorithms.hpp>

bool removable(const MessageNode& node)
{ return node.values.empty() && !node.hasChildren(); }

void cleanupNode(MessageNode& rootNode)
{
    for(auto it = rootNode.begin(); it != rootNode.end(); )
    {
        auto& child = *it;
        if(removable(child))
        {
            it = rootNode.erase(it);
        }
        else
        {
            it++;
        }
    }
}

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
        const Id<Process::ProcessModel>& proc)
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
            auto it = std::find_if(parent->begin(), parent->end(), [&] (const auto& other) { return &node == &other; });
            if(it != parent->end())
            {
                parent->erase(it);
                rec_delete(*parent);
            }
        }
    }
}


// Returns true if this node is to be deleted.
bool nodePruneAction_impl(
        MessageNode& node,
        const Id<Process::ProcessModel>& proc,
        QVector<ProcessStateData>& vec,
        const QVector<ProcessStateData>& other_vec)
{
    int vec_size = vec.size();
    if(vec_size > 1)
    {
        // We just remove the element
        // corresponding to this process.
        auto it = std::find_if(vec.begin(),
                               vec.end(),
                               [&] (const auto& data) {
            return data.process == proc;
        });

        if(it != vec.end())
        {
            vec.erase(it);
        }
    }
    else if(vec_size == 1)
    {
        // We are able to remove the whole node
        if(vec.front().process == proc
        && other_vec.isEmpty()
        && !node.values.userValue)
        {
            return true;
        }
        else
        {
            vec.clear();
            return false;
        }
    }

    return false;
}

void nodePruneAction(
        MessageNode& node,
        const Id<Process::ProcessModel>& proc,
        ProcessPosition pos
        )
{
    // If there is no corresponding message in our list,
    // but there is a corresponding process in the tree,
    // we prune it
    bool deleteMe = node.childCount() == 0;
    switch(pos)
    {
        case ProcessPosition::Previous:
        {
            deleteMe &= nodePruneAction_impl(
                        node, proc,
                        node.values.previousProcessValues,
                        node.values.followingProcessValues);
            break;
        }
        case ProcessPosition::Following:
        {
            deleteMe &= nodePruneAction_impl(
                        node, proc,
                        node.values.followingProcessValues,
                        node.values.previousProcessValues);
            break;
        }
        default:
            ISCORE_ABORT;
            break;
    }

    if(deleteMe)
    {
        node.values = StateNodeValues{};
    }
}

void nodeInsertAction(
        MessageNode& node,
        iscore::MessageList& msg,
        const Id<Process::ProcessModel>& proc,
        ProcessPosition pos
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
                case ProcessPosition::Previous:
                {
                    updateNode(node.values.previousProcessValues, mess.value, proc);
                    break;
                }
                case ProcessPosition::Following:
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

void rec_updateTree(
        MessageNode& node,
        iscore::MessageList& lst,
        const Id<Process::ProcessModel>& proc,
        ProcessPosition pos)
{
    // If the message is in the tree, we add the process value.
    int n = lst.size();
    nodeInsertAction(node, lst, proc, pos);
    if(lst.size() == n) // No nodes were added / updated
    {
        nodePruneAction(node, proc, pos);
    }

    for(auto& child : node)
    {
        rec_updateTree(child, lst, proc, pos);
    }

    cleanupNode(node);
}

// TODO another one to refactor with merges
// MergeFun takes a state node value and modifies it.
template<typename MergeFun>
void merge_impl(
        MessageNode& base,
        const iscore::Address& addr,
        MergeFun merge)
{
    QStringList path = toStringList(addr);

    ptr<MessageNode> node = &base;
    for(int i = 0; i < path.size(); i++)
    {
        auto it = std::find_if(
                    node->begin(), node->end(),
                    [&] (const auto& cur_node) {
            return cur_node.displayName() == path[i];
        });

        if(it == node->end())
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
                    StateNodeValues v;
                    merge(v);
                    newNode = &parentnode->emplace_back(
                                StateNodeData{
                                    path[k],
                                    std::move(v)},
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
                merge(node->values);
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
            mess.address,
            [&] (auto& nodeValues) {
            nodeValues.userValue = mess.value;
        });

    }
}


void updateTreeWithMessageList(
        MessageNode& rootNode,
        iscore::MessageList lst,
        const Id<Process::ProcessModel>& proc,
        ProcessPosition pos)
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
            rootNode, mess.address,
            [&] (StateNodeValues& nodeValues) {
            switch(pos)
            {
                case ProcessPosition::Previous:
                    nodeValues.previousProcessValues.push_back({proc, mess.value});
                    break;
                case ProcessPosition::Following:
                    nodeValues.followingProcessValues.push_back({proc, mess.value});
                    break;
                default:
                    ISCORE_ABORT;
                    break;
            }
        });
    }
}



void rec_pruneTree(
        MessageNode& node,
        const Id<Process::ProcessModel>& proc,
        ProcessPosition pos)
{
    // If the message is in the tree, we add the process value.
    nodePruneAction(node, proc, pos);

    for(auto& child : node)
    {
        rec_pruneTree(child, proc, pos);
    }

    cleanupNode(node);
}


void updateTreeWithRemovedProcess(
        MessageNode& rootNode,
        const Id<Process::ProcessModel>& proc,
        ProcessPosition pos)
{
    for(auto& child : rootNode)
    {
        rec_pruneTree(child, proc, pos);
    }

    cleanupNode(rootNode);
}






void nodePruneAction(
        MessageNode& node,
        ProcessPosition pos)
{
    // If there is no corresponding message in our list,
    // but there is a corresponding process in the tree,
    // we prune it
    bool deleteMe = node.childCount() == 0;
    switch(pos)
    {
        case ProcessPosition::Previous:
        {
            deleteMe &= !node.values.userValue && node.values.followingProcessValues.isEmpty();
            break;
        }
        case ProcessPosition::Following:
        {
            deleteMe &= !node.values.userValue && node.values.previousProcessValues.isEmpty();
            break;
        }
        default:
            ISCORE_ABORT;
            break;
    }

    if(deleteMe)
    {
        node.values = StateNodeValues{};
    }
}

void rec_pruneTree(
        MessageNode& node,
        ProcessPosition pos)
{
    nodePruneAction(node, pos);

    for(auto& child : node)
    {
        rec_pruneTree(child, pos);
    }

    cleanupNode(node);
}

void updateTreeWithRemovedConstraint(MessageNode& rootNode, ProcessPosition pos)
{
    for(auto& child : rootNode)
    {
        rec_pruneTree(child, pos);
    }
}

void updateTreeWithRemovedUserMessage(MessageNode& rootNode, const iscore::Address& addr)
{
    // Find the message node
    MessageNode* node = iscore::try_getNodeFromString(rootNode, stringList(addr));

    if(node)
    {
        node->values.userValue = iscore::OptionalValue{};

        // If it is empty, delete it.
        if(node->values.previousProcessValues.isEmpty()
        && node->values.followingProcessValues.isEmpty()
        && node->childCount() == 0)
        {
            rec_delete(*node);
        }
    }
}



void rec_removeUserValue(
        MessageNode& node)
{
    // Recursively set the user value to nil.
    node.values.userValue = iscore::OptionalValue{};

    for(auto& child : node)
    {
        rec_removeUserValue(child);
    }
}

bool rec_cleanup(
        MessageNode& node)
{
    std::set<const MessageNode*> toRemove;
    for(auto& child : node)
    {
        bool canEraseChild = rec_cleanup(child);
        if(canEraseChild)
        {
            toRemove.insert(&child);
        }
    }

    auto remove_it = remove_if(node,
                               [&] (const auto& child) {
        return toRemove.find(&child) != toRemove.end();
    });
    node.erase(remove_it, node.end());

    if(node.values.previousProcessValues.isEmpty()
    && node.values.followingProcessValues.isEmpty()
    && node.childCount() == 0)
    {
        return true;
    }

    return false;
}

void updateTreeWithRemovedNode(
        MessageNode& rootNode,
        const iscore::Address& addr)
{
    // Find the message node
    MessageNode* node = iscore::try_getNodeFromString(rootNode, stringList(addr));

    if(node)
    {
        // Recursively set the user value to nil.
        rec_removeUserValue(*node);

        // If it is empty, delete it
        rec_cleanup(*node);


        if(node->values.previousProcessValues.isEmpty()
        && node->values.followingProcessValues.isEmpty()
        && node->childCount() == 0)
        {
            node->parent()->erase(node->parent()->iterOfChild(node));
        }
    }
}
