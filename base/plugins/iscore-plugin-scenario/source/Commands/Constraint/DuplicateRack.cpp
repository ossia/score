#include "DuplicateRack.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace iscore;
using namespace Scenario::Command;

DuplicateRack::DuplicateRack(ObjectPath&& rackToCopy) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_rackPath {rackToCopy}
{
    auto& rack = m_rackPath.find<RackModel>();
    const auto& constraint = rack.constraint();

    m_newRackId = getStrongId(constraint.racks);
}

void DuplicateRack::undo() const
{
    auto& rack = m_rackPath.find<RackModel>();
    auto& constraint = rack.constraint();

    constraint.racks.remove(m_newRackId);
}

void DuplicateRack::redo() const
{
    auto& rack = m_rackPath.find<RackModel>();
    auto& constraint = rack.constraint();
    constraint.racks.add(new RackModel {rack,
                                    m_newRackId,
                                    &SlotModel::copyViewModelsInSameConstraint,
                                    &constraint});
}

void DuplicateRack::serializeImpl(QDataStream& s) const
{
    s << m_rackPath << m_newRackId;
}

void DuplicateRack::deserializeImpl(QDataStream& s)
{
    s >> m_rackPath >> m_newRackId;
}
