#include "SetMinDuration.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

SetMinDuration::SetMinDuration(ObjectPath&& constraintPath, TimeValue newDuration) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
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



void SetMinDuration::serializeImpl(QDataStream& s) const
{
    s << m_path << m_oldDuration << m_newDuration;
}

void SetMinDuration::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_oldDuration >> m_newDuration;
}
