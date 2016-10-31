#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <Process/TimeValue.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include "SetRigidity.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

namespace Scenario
{
namespace Command
{
// Rigid constraint == end TimeNode has a trigger

SetRigidity::SetRigidity(
        Path<ConstraintModel>&& constraintPath,
        bool rigid) :
    m_path {constraintPath},
    m_rigidity {rigid}
{
    // TODO make a class that embodies the logic for the relationship between rigidity and min/max.
    // Also, min/max are indicative if rigid, they can still be set but won't be used.
    auto& constraint = m_path.find();

    m_oldMinDuration = constraint.duration.minDuration();
    m_oldMaxDuration = constraint.duration.maxDuration();
    m_oldIsNull = constraint.duration.isMinNul();
    m_oldIsInfinite = constraint.duration.isMaxInfinite();
}

void SetRigidity::undo() const
{
    auto& constraint = m_path.find();
    constraint.duration.setRigid(!m_rigidity);

    constraint.duration.setMinNull(m_oldIsNull);
    constraint.duration.setMaxInfinite(m_oldIsInfinite);
    constraint.duration.setMinDuration(m_oldMinDuration);
    constraint.duration.setMaxDuration(m_oldMaxDuration);
}

void SetRigidity::redo() const
{
    auto& constraint = m_path.find();
    constraint.duration.setRigid(m_rigidity);
    //auto dur = constraint.duration.defaultDuration();

    if(m_rigidity)
    {
        constraint.duration.setMinNull(false);
        constraint.duration.setMaxInfinite(false);
    //    constraint.duration.setMinDuration(dur);
    //    constraint.duration.setMaxDuration(dur);
    }
    else
    {
        constraint.duration.setMinNull(true);
        constraint.duration.setMaxInfinite(true);
    }

}

void SetRigidity::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_oldMinDuration << m_oldMaxDuration << m_rigidity << m_oldIsNull << m_oldIsInfinite;
}

void SetRigidity::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_oldMinDuration >> m_oldMaxDuration >> m_rigidity >> m_oldIsNull >> m_oldIsInfinite;
}
}
}
