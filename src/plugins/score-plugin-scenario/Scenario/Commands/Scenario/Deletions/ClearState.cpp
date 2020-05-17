// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ClearState.hpp"

#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

namespace Scenario
{
namespace Command
{

ClearState::ClearState(const StateModel& state) : m_path{state}
{
  m_oldState = Process::getUserMessages(state.messages().rootNode());
}

void ClearState::undo(const score::DocumentContext& ctx) const
{
  auto& state = m_path.find(ctx);

  Process::MessageNode n = state.messages().rootNode();
  updateTreeWithMessageList(n, m_oldState);

  state.messages() = std::move(n);
}

void ClearState::redo(const score::DocumentContext& ctx) const
{
  auto& state = m_path.find(ctx);

  Process::MessageNode n = state.messages().rootNode();
  removeAllUserMessages(n);
  state.messages() = std::move(n);
}

void ClearState::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_oldState;
}

void ClearState::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_oldState;
}
}
}
