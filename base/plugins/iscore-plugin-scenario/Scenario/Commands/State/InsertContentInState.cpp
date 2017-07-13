// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonArray>
#include <QJsonValue>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <algorithm>

#include "InsertContentInState.hpp"
#include <Process/State/MessageNode.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/tree/TreeNode.hpp>

namespace Scenario
{
namespace Command
{

InsertContentInState::InsertContentInState(
    const QJsonObject& stateData,
    const Scenario::StateModel& state)
    : m_state{state}
{
  // TODO ask what should be copied ? the state due to the processes ? the user
  // state ?
  // For now we copy the whole value.
  // First recreate the tree

  // TODO we should update the processes here, and provide an API to do this
  // properly.

  m_oldNode = state.messages().rootNode();
  m_newNode = m_oldNode;
  updateTreeWithMessageList(
      m_newNode,
      Process::flatten(
          iscore::unmarshall<Process::MessageNode>(stateData["Messages"].toObject())));
}

void InsertContentInState::undo(const iscore::DocumentContext& ctx) const
{
  auto& state = m_state.find(ctx);
  state.messages() = m_oldNode;
}

void InsertContentInState::redo(const iscore::DocumentContext& ctx) const
{
  auto& state = m_state.find(ctx);
  state.messages() = m_newNode;
}

void InsertContentInState::serializeImpl(DataStreamInput& s) const
{
  s << m_oldNode << m_newNode << m_state;
}

void InsertContentInState::deserializeImpl(DataStreamOutput& s)
{
  s >> m_oldNode >> m_newNode >> m_state;
}
}
}
