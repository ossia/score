#include "AddLayerInNewSlot.hpp"

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>

#include <Process/Process.hpp>
#include <Process/LayerModel.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>


using namespace iscore;
using namespace Scenario::Command;

AddLayerInNewSlot::AddLayerInNewSlot(
        Path<ConstraintModel>&& constraintPath,
        const Id<Process>& process) :
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_path {std::move(constraintPath) },
    m_sharedProcessModelId{process}
{
    auto& constraint = m_path.find();

    if(constraint.racks.empty())
    {
        m_createdRackId = getStrongId(constraint.racks);
        m_existingRack = false;
    }
    else
    {
        m_createdRackId = (*constraint.racks.begin()).id();
        m_existingRack = true;
    }

    m_createdSlotId = Id<SlotModel> (iscore::id_generator::getFirstId());
    m_createdLayerId = Id<LayerModel> (iscore::id_generator::getFirstId());
    m_processData = constraint.processes.at(m_sharedProcessModelId).makeLayerConstructionData();
}

void AddLayerInNewSlot::undo() const
{
    auto& constraint = m_path.find();
    auto& rack = constraint.racks.at(m_createdRackId);

    // Removing the slot is enough
    rack.slotmodels.remove(m_createdSlotId);

    // Remove the rack
    if(!m_existingRack)
    {
        constraint.racks.remove(m_createdRackId);
    }
}

void AddLayerInNewSlot::redo() const
{
    auto& constraint = m_path.find();

    // Rack
    if(!m_existingRack)
    {
        // TODO refactor with AddRackToConstraint
        auto rack = new RackModel{m_createdRackId, &constraint};
        constraint.racks.add(rack);

        // If it is the first rack created,
        // it is also assigned to all the views of the constraint.
        if(constraint.racks.size() == 1)
        {
            for(const auto& vm : constraint.viewModels())
            {
                vm->showRack(m_createdRackId);
            }
        }
    }

    // Slot
    auto& rack = constraint.racks.at(m_createdRackId);
    rack.addSlot(new SlotModel {m_createdSlotId,
                                &rack});

    // Process View
    auto& slot = rack.slotmodels.at(m_createdSlotId);
    auto& proc = constraint.processes.at(m_sharedProcessModelId);

    slot.layers.add(proc.makeLayer(m_createdLayerId,
                                      m_processData,
                                      &slot));
}

void AddLayerInNewSlot::serializeImpl(QDataStream& s) const
{
    s << m_path
      << m_existingRack
      << m_processId
      << m_createdRackId
      << m_createdSlotId
      << m_createdLayerId
      << m_sharedProcessModelId
      << m_processData;
}

void AddLayerInNewSlot::deserializeImpl(QDataStream& s)
{
    s >> m_path
      >> m_existingRack
      >> m_processId
      >> m_createdRackId
      >> m_createdSlotId
      >> m_createdLayerId
      >> m_sharedProcessModelId
      >> m_processData;
}
