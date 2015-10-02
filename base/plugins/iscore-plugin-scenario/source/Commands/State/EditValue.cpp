#include "EditValue.hpp"
#include <Document/State/StateModel.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <ProcessInterface/State/ProcessStateDataInterface.hpp>
#include <Document/State/ItemModel/MessageItemModelAlgorithms.hpp>

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

        updateTreeWithMessageList(m_newState, lst, prevProc->process().id(), Position::Previous);
    }
    for(ProcessStateDataInterface* prevProc : sm.followingProcesses())
    {
        auto lst = prevProc->setMessages(mess, m_oldState);

        updateTreeWithMessageList(m_newState, lst, prevProc->process().id(), Position::Following);
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
    d << m_path
      << m_nodePath
      << m_oldState
      << m_newState;
}

void EditValue::deserializeImpl(QDataStream &d)
{
    d >> m_path
      >> m_nodePath
      >> m_oldState
      >> m_newState;
}
