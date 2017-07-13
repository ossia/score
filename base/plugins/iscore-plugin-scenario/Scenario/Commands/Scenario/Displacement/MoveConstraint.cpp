// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <algorithm>

#include "MoveConstraint.hpp"
#include <Process/TimeValue.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
MoveConstraint::MoveConstraint(
    const Scenario::ProcessModel& scenar,
    Id<ConstraintModel> id,
    double height)
    : m_path{scenar}, m_constraint{id}, m_newHeight{height}
{
  auto& cst = scenar.constraints.at(m_constraint);
  m_oldHeight = cst.heightPercentage();

  auto list = selectedElements(scenar.constraints);

  for (auto& elt : list)
  {
    m_selectedConstraints.append({elt->id(), elt->heightPercentage()});
  }

  if (m_selectedConstraints.empty())
    m_selectedConstraints.append({m_constraint, m_oldHeight});
}

void MoveConstraint::undo(const iscore::DocumentContext& ctx) const
{
  auto& scenar = m_path.find(ctx);
  for (const auto& cstr : m_selectedConstraints)
  {
    updateConstraintVerticalPos(cstr.second, cstr.first, scenar);
  }
}

void MoveConstraint::redo(const iscore::DocumentContext& ctx) const
{
  auto& scenar = m_path.find(ctx);
  for (const auto& cstr : m_selectedConstraints)
  {
    updateConstraintVerticalPos(
        cstr.second + m_newHeight - m_oldHeight, cstr.first, scenar);
  }
}

void MoveConstraint::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_constraint << m_oldHeight << m_newHeight
    << m_selectedConstraints;
}

void MoveConstraint::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_constraint >> m_oldHeight >> m_newHeight
      >> m_selectedConstraints;
}
}
}
