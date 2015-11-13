#include "UpdateState.hpp"
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>

AddMessagesToState::AddMessagesToState(
        Path<MessageItemModel> &&device_tree,
        const iscore::MessageList& messages):
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

        // TODO backup all the processes, not just the messages.

        // For all the nodes in m_newState :
        // - if there is a new message in lst, add it as "process" node in the tree
        // - if there is a message both in lst and in the tree, update it in the tree
        // - if there is a message in the tree with the process, but not in lst it means
        // that the process stopped using this address. Hence we remove the "process" part
        // of this node, and if there is no "user" or message from another process, we remove
        // the node altogether

        for(ProcessStateDataInterface* prevProc : sm.previousProcesses())
        {
            m_previousBackup.insert(prevProc->process().id(), prevProc->messages());

            auto lst = prevProc->setMessages(messages, m_oldState);

            updateTreeWithMessageList(m_newState, lst, prevProc->process().id(), ProcessPosition::Previous);
        }
        for(ProcessStateDataInterface* prevProc : sm.followingProcesses())
        {
            m_followingBackup.insert(prevProc->process().id(), prevProc->messages());

            auto lst = prevProc->setMessages(messages, m_oldState);

            updateTreeWithMessageList(m_newState, lst, prevProc->process().id(), ProcessPosition::Following);
        }

        // TODO one day there will also be State functions that will perform
        // some local computation.
    }

    updateTreeWithMessageList(m_newState, messages);
}

void AddMessagesToState::undo() const
{
    auto& model = m_path.find();
    model = m_oldState;

    // Restore the state of the processes.
    auto& sm = model.stateModel;
    for(ProcessStateDataInterface* prevProc : sm.previousProcesses())
    {
        prevProc->setMessages(m_previousBackup[prevProc->process().id()], m_oldState);
    }
    for(ProcessStateDataInterface* prevProc : sm.followingProcesses())
    {
        prevProc->setMessages(m_followingBackup[prevProc->process().id()], m_oldState);
    }
}

void AddMessagesToState::redo() const
{
    auto& model = m_path.find();
    model = m_newState;
}

void AddMessagesToState::serializeImpl(QDataStream &d) const
{
    d << m_path
      << m_oldState
      << m_newState
      << m_previousBackup
      << m_followingBackup;
}

void AddMessagesToState::deserializeImpl(QDataStream &d)
{
    d >> m_path
      >> m_oldState
      >> m_newState
      >> m_previousBackup
      >> m_followingBackup;
}
