#include "SetRigidity.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

SetRigidity::SetRigidity(ObjectPath&& constraintPath, bool rigid) :
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
        auto& constraint = m_path.find<ConstraintModel>();
        Q_ASSERT(constraint.isRigid() != rigid);

        m_oldMinDuration = constraint.minDuration();
        m_oldMaxDuration = constraint.maxDuration();
    }
}

void SetRigidity::undo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    constraint.setRigid(!m_rigidity);

    if(m_rigidity)
    {
        constraint.setMinDuration(m_oldMinDuration);
        constraint.setMaxDuration(m_oldMaxDuration);
    }
    else
    {
        constraint.setMinDuration(constraint.defaultDuration());
        constraint.setMaxDuration(constraint.defaultDuration());
    }
}

void SetRigidity::redo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    constraint.setRigid(m_rigidity);

    if(m_rigidity)
    {
        constraint.setMinDuration(constraint.defaultDuration());
        constraint.setMaxDuration(constraint.defaultDuration());
    }
    else
    {
        constraint.setMinDuration(TimeValue(std::chrono::milliseconds(0)));
        constraint.setMaxDuration(TimeValue(PositiveInfinity{}));
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
