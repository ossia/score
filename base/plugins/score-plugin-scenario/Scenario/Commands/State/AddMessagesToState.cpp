// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/State/ProcessStateDataInterface.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <QDataStream>
#include <QtGlobal>
#include <algorithm>

#include "AddMessagesToState.hpp"
#include <Process/Process.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/tree/TreeNode.hpp>

namespace Scenario
{
namespace Command
{
AddMessagesToState::AddMessagesToState(
    const Scenario::StateModel& state,
    const State::MessageList& messages)
    : m_path{state}
{
  m_oldState = state.messages().rootNode();
  m_newState = m_oldState;

  // TODO backup all the processes, not just the messages.

  // For all the nodes in m_newState :
  // - if there is a new message in lst, add it as "process" node in the tree
  // - if there is a message both in lst and in the tree, update it in the
  // tree
  // - if there is a message in the tree with the process, but not in lst it
  // means
  // that the process stopped using this address. Hence we remove the
  // "process" part
  // of this node, and if there is no "user" or message from another process,
  // we remove
  // the node altogether

  // TODO this won't work in network editing ???
  for (const ProcessStateWrapper& prevProc : state.previousProcesses())
  {
    const auto& processModel = prevProc.process().process();
    m_previousBackup.insert(
          processModel.id(), prevProc.process().messages());

    auto lst = prevProc.process().setMessages(messages, m_oldState);

    updateTreeWithMessageList(
          m_newState, lst, processModel.id(), ProcessPosition::Previous);
  }

  for (const ProcessStateWrapper& nextProc : state.followingProcesses())
  {
    const auto& processModel = nextProc.process().process();
    m_followingBackup.insert(
          processModel.id(), nextProc.process().messages());

    auto lst = nextProc.process().setMessages(messages, m_oldState);

    updateTreeWithMessageList(
          m_newState, lst, processModel.id(), ProcessPosition::Following);
  }

  // TODO one day there will also be State functions that will perform
  // some local computation.
  // TODO this day has come
  // WARNING we are in the future


  updateTreeWithMessageList(m_newState, messages);
}

void AddMessagesToState::undo(const score::DocumentContext& ctx) const
{
  auto& model = m_path.find(ctx).messages();
  model = m_oldState;

  // Restore the state of the processes.
  auto& sm = model.stateModel;
  for (const ProcessStateWrapper& prevProc : sm.previousProcesses())
  {
    prevProc.process().setMessages(
        m_previousBackup[prevProc.process().process().id()], m_oldState);
  }
  for (const ProcessStateWrapper& nextProc : sm.followingProcesses())
  {
    nextProc.process().setMessages(
        m_followingBackup[nextProc.process().process().id()], m_oldState);
  }
}

void AddMessagesToState::redo(const score::DocumentContext& ctx) const
{
  auto& model = m_path.find(ctx).messages();
  model = m_newState;
}

void AddMessagesToState::serializeImpl(DataStreamInput& d) const
{
  d << m_path << m_oldState << m_newState << m_previousBackup
    << m_followingBackup;
}

void AddMessagesToState::deserializeImpl(DataStreamOutput& d)
{
  d >> m_path >> m_oldState >> m_newState >> m_previousBackup
      >> m_followingBackup;
}
}
}
