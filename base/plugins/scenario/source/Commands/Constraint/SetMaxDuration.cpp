#include "SetMaxDuration.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

#define CMD_NAME "SetMaxDuration"
#define CMD_DESC QObject::tr("Set max duration of constraint")

SetMaxDuration::SetMaxDuration() :
    SerializableCommand {"ScenarioControl",
    CMD_NAME,
    CMD_DESC
}
{
}

SetMaxDuration::SetMaxDuration(ObjectPath&& constraintPath, TimeValue newDuration) :
    SerializableCommand {"ScenarioControl",
    CMD_NAME,
    CMD_DESC
},
m_path {constraintPath},
m_oldDuration {m_path.find<ConstraintModel>()->maxDuration() },
m_newDuration {newDuration}
{
}

void SetMaxDuration::undo()
{
    auto constraint = m_path.find<ConstraintModel>();
    constraint->setMaxDuration(m_oldDuration);
}

void SetMaxDuration::redo()
{
    auto constraint = m_path.find<ConstraintModel>();
    constraint->setMaxDuration(m_newDuration);
}

int SetMaxDuration::id() const
{
    return canMerge() ? uid() : -1;
}

bool SetMaxDuration::mergeWith(const QUndoCommand* other)
{
    if(other->id() != id())
    {
        return false;
    }

    auto cmd = static_cast<const SetMaxDuration*>(other);
    m_newDuration = cmd->m_newDuration;

    return true;
}

void SetMaxDuration::serializeImpl(QDataStream& s) const
{
    s << m_path << m_oldDuration << m_newDuration;
}

void SetMaxDuration::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_oldDuration >> m_newDuration;
}
