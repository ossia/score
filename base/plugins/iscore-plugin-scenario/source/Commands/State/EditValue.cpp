#include "EditValue.hpp"
#include <Document/State/StateModel.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <ProcessInterface/State/ProcessStateDataInterface.hpp>
#include <Document/State/ItemModel/MessageItemModelAlgorithms.hpp>

UpdateState::UpdateState(
        Path<MessageItemModel> &&device_tree,
        const iscore::MessageList& messages):
    iscore::SerializableCommand{factoryName(),
                                commandName(),
                                description()},
    m_path{device_tree}
{
    auto model = m_path.try_find();
    if(model)
    {
        m_oldState = model->rootNode();
    }

    m_newState = m_oldState;

    if(model)
    {
        auto& sm = model->stateModel;

        // For all the nodes in m_newState :
        // - if there is a new message in lst, add it as "process" node in the tree
        // - if there is a message both in lst and in the tree, update it in the tree
        // - if there is a message in the tree with the process, but not in lst it means
        // that the process stopped using this address. Hence we remove the "process" part
        // of this node, and if there is no "user" or message from another process, we remove
        // the node altogether

        for(ProcessStateDataInterface* prevProc : sm.previousProcesses())
        {
            auto lst = prevProc->setMessages(messages, m_oldState);

            updateTreeWithMessageList(m_newState, lst, prevProc->process().id(), Position::Previous);
        }
        for(ProcessStateDataInterface* prevProc : sm.followingProcesses())
        {
            auto lst = prevProc->setMessages(messages, m_oldState);

            updateTreeWithMessageList(m_newState, lst, prevProc->process().id(), Position::Following);
        }

        updateTreeWithMessageList(m_newState, messages);
        // TODO one day there will also be State functions that will perform
        // some local computation.
    }
    else
    {
        // Just merge
        ISCORE_TODO;
    }
}

void UpdateState::undo()
{
    auto& model = m_path.find();
    model = m_oldState;
}

void UpdateState::redo()
{
    auto& model = m_path.find();
    model = m_newState;
}

void UpdateState::serializeImpl(QDataStream &d) const
{
    d << m_path
      << m_nodePath
      << m_oldState
      << m_newState;
}

void UpdateState::deserializeImpl(QDataStream &d)
{
    d >> m_path
      >> m_nodePath
      >> m_oldState
      >> m_newState;
}
