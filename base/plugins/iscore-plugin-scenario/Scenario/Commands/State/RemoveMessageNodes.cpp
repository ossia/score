// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <algorithm>

#include "RemoveMessageNodes.hpp"
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/tree/TreeNode.hpp>
namespace Scenario
{
namespace Command
{

RemoveMessageNodes::RemoveMessageNodes(
    const Scenario::StateModel& model,
    const std::vector<const Process::MessageNode*>& nodes)
    : m_path{model}
{
  m_oldState = model.messages().rootNode();
  m_newState = m_oldState;
  for (const auto& node : nodes)
  {
    updateTreeWithRemovedNode(m_newState, address(*node).address);
  }
}

void RemoveMessageNodes::undo(const iscore::DocumentContext& ctx) const
{
  auto& model = m_path.find(ctx).messages();
  model = m_oldState;
}

void RemoveMessageNodes::redo(const iscore::DocumentContext& ctx) const
{
  auto& model = m_path.find(ctx).messages();
  model = m_newState;
}

void RemoveMessageNodes::serializeImpl(DataStreamInput& d) const
{
  d << m_path << m_oldState << m_newState;
}

void RemoveMessageNodes::deserializeImpl(DataStreamOutput& d)
{
  d >> m_path >> m_oldState >> m_newState;
}
}
}
