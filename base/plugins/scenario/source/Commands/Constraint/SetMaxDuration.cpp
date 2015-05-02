#include "SetMaxDuration.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

SetMaxDuration::SetMaxDuration(ObjectPath&& constraintPath, TimeValue newDuration) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {constraintPath},
    m_oldDuration {m_path.find<ConstraintModel>().maxDuration() },
    m_newDuration {newDuration}
{
}

void SetMaxDuration::undo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    constraint.setMaxDuration(m_oldDuration);
}

void SetMaxDuration::redo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    constraint.setMaxDuration(m_newDuration);
}



void SetMaxDuration::serializeImpl(QDataStream& s) const
{
    s << m_path << m_oldDuration << m_newDuration;
}

void SetMaxDuration::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_oldDuration >> m_newDuration;
}
