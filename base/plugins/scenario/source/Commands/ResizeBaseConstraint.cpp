#include "ResizeBaseConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include <ProcessInterface/TimeValue.hpp>
#include <iscore/document/DocumentInterface.hpp>

using namespace iscore;
using namespace Scenario::Command;
using namespace iscore::IDocument;

ResizeBaseConstraint::ResizeBaseConstraint(ObjectPath&& constraintPath,
        TimeValue duration) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {constraintPath},
    m_newDuration {duration}
{
    auto constraint = m_path.find<ConstraintModel>();
    m_oldDuration = constraint->defaultDuration();
}

void ResizeBaseConstraint::undo()
{
    auto constraint = m_path.find<ConstraintModel>();
    constraint->setDefaultDuration(m_oldDuration);
}

void ResizeBaseConstraint::redo()
{
    auto constraint = m_path.find<ConstraintModel>();
    constraint->setDefaultDuration(m_newDuration);
}

bool ResizeBaseConstraint::mergeWith(const Command* other)
{
    if(other->uid() != uid())
    {
        return false;
    }

    auto cmd = static_cast<const ResizeBaseConstraint*>(other);
    m_newDuration = cmd->m_newDuration;

    return true;
}

void ResizeBaseConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_oldDuration << m_newDuration;
}

void ResizeBaseConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_oldDuration >> m_newDuration;
}
