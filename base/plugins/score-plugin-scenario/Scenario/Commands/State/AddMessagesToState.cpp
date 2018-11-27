// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddMessagesToState.hpp"

#include <Process/Process.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <QDataStream>
#include <QtGlobal>

#include <algorithm>

namespace Scenario
{
namespace Command
{
AddMessagesToState::AddMessagesToState(
    const Scenario::StateModel& state, const State::MessageList& messages)
    : m_path{state}
{
  m_oldState = state.messages().rootNode();
  m_newState = m_oldState;

  // TODO backup all the processes, not just the messages.

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
}

void AddMessagesToState::redo(const score::DocumentContext& ctx) const
{
  auto& model = m_path.find(ctx).messages();
  model = m_newState;
}

void AddMessagesToState::serializeImpl(DataStreamInput& d) const
{
  d << m_path << m_oldState << m_newState;
}

void AddMessagesToState::deserializeImpl(DataStreamOutput& d)
{
  d >> m_path >> m_oldState >> m_newState;
}

ReplaceMessagesInState::ReplaceMessagesInState(
    const Scenario::StateModel& state, State::MessageList&& messages)
    : m_path{state}
    , m_oldState{state.messages().rootNode()}
    , m_newState{std::move(messages)}
{
}

void ReplaceMessagesInState::undo(const score::DocumentContext& ctx) const
{
  auto& model = m_path.find(ctx).messages();
  model = m_oldState;
}

void ReplaceMessagesInState::redo(const score::DocumentContext& ctx) const
{
  auto& model = m_path.find(ctx).messages();
  model = m_newState;
}

void ReplaceMessagesInState::serializeImpl(DataStreamInput& d) const
{
  d << m_path << m_oldState << m_newState;
}

void ReplaceMessagesInState::deserializeImpl(DataStreamOutput& d)
{
  d >> m_path >> m_oldState >> m_newState;
}
}
}
