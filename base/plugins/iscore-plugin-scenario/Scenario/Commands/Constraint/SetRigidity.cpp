#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include "SetRigidity.hpp"
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
// Rigid constraint == end TimeNode has a trigger

SetRigidity::SetRigidity(const ConstraintModel& constraint, bool rigid)
    : m_path{constraint}, m_rigidity{rigid}
{
  // TODO make a class that embodies the logic for the relationship between
  // rigidity and min/max.
  // Also, min/max are indicative if rigid, they can still be set but won't be
  // used.
  m_oldMinDuration = constraint.duration.minDuration();
  m_oldMaxDuration = constraint.duration.maxDuration();
  m_oldIsNull = constraint.duration.isMinNull();
  m_oldIsInfinite = constraint.duration.isMaxInfinite();
}

void SetRigidity::undo(const iscore::DocumentContext& ctx) const
{
  auto& constraint = m_path.find(ctx);
  constraint.duration.setRigid(!m_rigidity);

  constraint.duration.setMinNull(m_oldIsNull);
  constraint.duration.setMaxInfinite(m_oldIsInfinite);
  constraint.duration.setMinDuration(m_oldMinDuration);
  constraint.duration.setMaxDuration(m_oldMaxDuration);
}

void SetRigidity::redo(const iscore::DocumentContext& ctx) const
{
  auto& constraint = m_path.find(ctx);
  constraint.duration.setRigid(m_rigidity);
  auto dur = constraint.duration.defaultDuration();

  if (m_rigidity)
  {
    constraint.duration.setMinNull(false);
    constraint.duration.setMaxInfinite(false);
    constraint.duration.setMinDuration(dur);
    constraint.duration.setMaxDuration(dur);
  }
  else
  {
    constraint.duration.setMinNull(true);
    constraint.duration.setMaxInfinite(true);
  }
}

void SetRigidity::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_oldMinDuration << m_oldMaxDuration << m_rigidity
    << m_oldIsNull << m_oldIsInfinite;
}

void SetRigidity::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_oldMinDuration >> m_oldMaxDuration >> m_rigidity
      >> m_oldIsNull >> m_oldIsInfinite;
}
}
}
