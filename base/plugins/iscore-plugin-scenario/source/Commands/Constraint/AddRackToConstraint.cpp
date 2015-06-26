#include "AddRackToConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace iscore;
using namespace Scenario::Command;

AddRackToConstraint::AddRackToConstraint(ObjectPath&& constraintPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {constraintPath}
{
    auto& constraint = m_path.find<ConstraintModel>();
    m_createdRackId = getStrongId(constraint.rackes());
}

void AddRackToConstraint::undo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    constraint.removeRack(m_createdRackId);
}

void AddRackToConstraint::redo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    auto rack = new RackModel{m_createdRackId, &constraint};

    constraint.addRack(rack);
    rack->metadata.setName(QString{"Rack.%1"}.arg(constraint.rackes().size()));

    // If it is the first rack created,
    // it is also assigned to the full view of the constraint.
    if(constraint.rackes().size() == 1)
    {
        constraint.fullView()->showRack(m_createdRackId);
    }
}

void AddRackToConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_createdRackId;
}

void AddRackToConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_createdRackId;
}
