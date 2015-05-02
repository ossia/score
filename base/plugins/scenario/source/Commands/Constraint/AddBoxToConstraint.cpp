#include "AddBoxToConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddBoxToConstraint::AddBoxToConstraint(ObjectPath&& constraintPath) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {constraintPath}
{
    auto& constraint = m_path.find<ConstraintModel>();
    m_createdBoxId = getStrongId(constraint.boxes());
}

void AddBoxToConstraint::undo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    constraint.removeBox(m_createdBoxId);
}

void AddBoxToConstraint::redo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    auto box = new BoxModel{m_createdBoxId, &constraint};

    constraint.addBox(box);
    box->metadata.setName(QString{"Box.%1"}.arg(constraint.boxes().size()));

    // If it is the first box created,
    // it is also assigned to the full view of the constraint.
    if(constraint.boxes().size() == 1)
    {
        constraint.fullView()->showBox(m_createdBoxId);
    }
}

void AddBoxToConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_createdBoxId;
}

void AddBoxToConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_createdBoxId;
}
