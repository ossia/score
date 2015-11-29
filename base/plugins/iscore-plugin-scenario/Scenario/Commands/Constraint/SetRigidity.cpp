#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <Process/TimeValue.hpp>
#include "Scenario/Document/Constraint/ConstraintDurations.hpp"
#include "SetRigidity.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

using namespace iscore;
using namespace Scenario::Command;

// Rigid constraint == end TimeNode has a trigger

SetRigidity::SetRigidity(
        Path<ConstraintModel>&& constraintPath,
        bool rigid) :
    m_path {constraintPath},
    m_rigidity {rigid}
{
    // We suppose that this command is never called with rigid == current state of the constraint.
//    if(rigid)  // it is currently not rigid so min & max are set -> TODO : WHY ??
    // TODO make a class that embodies the logic for the relationship between rigidity and min/max.
    // Also, min/max are indicative if rigid, they can still be set but won't be used.
    auto& constraint = m_path.find();
    ISCORE_ASSERT(constraint.duration.isRigid() != rigid);

    m_oldMinDuration = constraint.duration.minDuration();
    m_oldMaxDuration = constraint.duration.maxDuration();
}

void SetRigidity::undo() const
{
    auto& constraint = m_path.find();
    constraint.duration.setRigid(!m_rigidity);

    if(m_rigidity)
    {
        constraint.duration.setMinDuration(m_oldMinDuration);
        constraint.duration.setMaxDuration(m_oldMaxDuration);
    }
    else
    {
        constraint.duration.setMinDuration(constraint.duration.defaultDuration());
        constraint.duration.setMaxDuration(constraint.duration.defaultDuration());
    }
}

void SetRigidity::redo() const
{
    auto& constraint = m_path.find();
    constraint.duration.setRigid(m_rigidity);
    auto dur = constraint.duration.defaultDuration();

    if(m_rigidity)
    {
        constraint.duration.setMinDuration(constraint.duration.defaultDuration());
        constraint.duration.setMaxDuration(constraint.duration.defaultDuration());
    }
    else
    {
        constraint.duration.setMinDuration( TimeValue::fromMsecs(0.8 * dur.msec()));
        constraint.duration.setMaxDuration( TimeValue::fromMsecs(1.2 * dur.msec()));
    }

}

void SetRigidity::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_rigidity << m_oldMinDuration << m_oldMaxDuration;
}

void SetRigidity::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_rigidity >> m_oldMinDuration >> m_oldMaxDuration;
}
