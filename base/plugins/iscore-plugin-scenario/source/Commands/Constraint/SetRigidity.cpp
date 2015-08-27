#include "SetRigidity.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

SetRigidity::SetRigidity(
        ModelPath<ConstraintModel>&& constraintPath,
        bool rigid) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {constraintPath},
    m_rigidity {rigid}
{
    // We suppose that this command is never called with rigid == current state of the constraint.
//    if(rigid)  // it is currently not rigid so min & max are set -> TODO : WHY ??
    // TODO make a class that embodies the logic for the relationship between rigidity and min/max.
    // Also, min/max are indicative if rigid, they can still be set but won't be used.
    {
        auto& constraint = m_path.find();
        ISCORE_ASSERT(constraint.duration.isRigid() != rigid);

        m_oldMinDuration = constraint.duration.minDuration();
        m_oldMaxDuration = constraint.duration.maxDuration();
    }
}

void SetRigidity::undo()
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

void SetRigidity::redo()
{
    auto& constraint = m_path.find();
    constraint.duration.setRigid(m_rigidity);

    if(m_rigidity)
    {
        constraint.duration.setMinDuration(constraint.duration.defaultDuration());
        constraint.duration.setMaxDuration(constraint.duration.defaultDuration());
    }
    else
    {
        constraint.duration.setMinDuration(TimeValue(std::chrono::milliseconds(0)));
        constraint.duration.setMaxDuration(TimeValue(PositiveInfinity{}));
    }

}

void SetRigidity::serializeImpl(QDataStream& s) const
{
    s << m_path << m_rigidity << m_oldMinDuration << m_oldMaxDuration;
}

void SetRigidity::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_rigidity >> m_oldMinDuration >> m_oldMaxDuration;
}
