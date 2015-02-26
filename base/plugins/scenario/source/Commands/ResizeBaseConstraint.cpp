#include "ResizeBaseConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include <ProcessInterface/TimeValue.hpp>
#include <core/interface/document/DocumentInterface.hpp>
#include <QApplication>

// TODO changer gestion de UID
#define CMD_UID 1025

using namespace iscore;
using namespace Scenario::Command;
using namespace iscore::IDocument;

ResizeBaseConstraint::ResizeBaseConstraint() :
    SerializableCommand {"ScenarioControl",
    "ResizeBaseConstraint",
    QObject::tr("Set default duration of constraint")
}
{

}

ResizeBaseConstraint::ResizeBaseConstraint(ObjectPath&& constraintPath,
        TimeValue duration) :
    SerializableCommand {"ScenarioControl",
    "ResizeBaseConstraint",
    QObject::tr("Set default duration of constraint")
},
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

int ResizeBaseConstraint::id() const
{
    return CMD_UID;
}

bool ResizeBaseConstraint::mergeWith(const QUndoCommand* other)
{
    if(other->id() != id())
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
