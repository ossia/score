#include "SetRigidity.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

SetRigidity::SetRigidity(ObjectPath&& constraintPath, bool rigid) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_path {constraintPath},
m_rigidity {rigid}
{
    // We suppose that this command is never called with rigid == current state of the constraint.
    if(rigid)  // it is currently not rigid so min & max are set
    {
        auto constraint = m_path.find<ConstraintModel>();
        m_oldMinDuration = constraint->minDuration();
        m_oldMaxDuration = constraint->maxDuration();
    }
}

void SetRigidity::undo()
{
    auto constraint = m_path.find<ConstraintModel>();

    if(m_rigidity)
    {
        constraint->setMinDuration(m_oldMinDuration);
        constraint->setMaxDuration(m_oldMaxDuration);
    }
    else
    {
        constraint->setMinDuration(constraint->defaultDuration());
        constraint->setMaxDuration(constraint->defaultDuration());
    }
}

void SetRigidity::redo()
{
    auto constraint = m_path.find<ConstraintModel>();

    if(m_rigidity)
    {
        constraint->setMinDuration(constraint->defaultDuration());
        constraint->setMaxDuration(constraint->defaultDuration());
    }
    else
    {
        // TODO find a better default ? (and be careful with min < 0
        auto percentage = constraint->defaultDuration() * 0.1;
        constraint->setMinDuration(constraint->defaultDuration() - percentage);
        constraint->setMaxDuration(constraint->defaultDuration() + percentage);
    }
}

int SetRigidity::id() const
{
    return 1;
}

bool SetRigidity::mergeWith(const QUndoCommand* other)
{
    return false;
}

void SetRigidity::serializeImpl(QDataStream& s) const
{
    s << m_path << m_rigidity << m_oldMinDuration << m_oldMaxDuration;
}

void SetRigidity::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_rigidity >> m_oldMinDuration >> m_oldMaxDuration;
}
