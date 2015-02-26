#include "SetMinDuration.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

using namespace iscore;
using namespace Scenario::Command;
// TODO changer gestion de UID
#define CMD_UID 1000
#define CMD_NAME "SetMinDuration"
#define CMD_DESC QObject::tr("Set min duration of constraint")

SetMinDuration::SetMinDuration() :
    SerializableCommand {"ScenarioControl",
    CMD_NAME,
    CMD_DESC
}
{
}

SetMinDuration::SetMinDuration(ObjectPath&& constraintPath, TimeValue newDuration) :
    SerializableCommand {"ScenarioControl",
    CMD_NAME,
    CMD_DESC
},
m_path {constraintPath},
m_oldDuration {m_path.find<ConstraintModel>()->minDuration() },
m_newDuration {newDuration}
{
}

void SetMinDuration::undo()
{
    auto constraint = m_path.find<ConstraintModel>();
    constraint->setMinDuration(m_oldDuration);
}

void SetMinDuration::redo()
{
    auto constraint = m_path.find<ConstraintModel>();
    constraint->setMinDuration(m_newDuration);
}

int SetMinDuration::id() const
{
    return canMerge() ? CMD_UID : -1;
}

bool SetMinDuration::mergeWith(const QUndoCommand* other)
{
    if(other->id() != id())
    {
        return false;
    }

    auto cmd = static_cast<const SetMinDuration*>(other);
    m_newDuration = cmd->m_newDuration;

    return true;
}

void SetMinDuration::serializeImpl(QDataStream& s) const
{
    s << m_path << m_oldDuration << m_newDuration;
}

void SetMinDuration::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_oldDuration >> m_newDuration;
}
