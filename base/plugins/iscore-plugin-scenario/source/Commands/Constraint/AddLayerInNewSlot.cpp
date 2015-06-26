#include "AddLayerInNewSlot.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"

#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/LayerModel.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>


using namespace iscore;
using namespace Scenario::Command;

AddLayerInNewSlot::AddLayerInNewSlot(ObjectPath&& constraintPath,
                                                 id_type<ProcessModel> process) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {std::move(constraintPath) },
    m_sharedProcessModelId{process}
{
    auto& constraint = m_path.find<ConstraintModel>();

    if(constraint.racks().empty())
    {
        m_createdRackId = getStrongId(constraint.racks());
        m_existingRack = false;
    }
    else
    {
        m_createdRackId = (*constraint.racks().begin())->id();
        m_existingRack = true;
    }

    m_createdSlotId = id_type<SlotModel> (getNextId());
    m_createdLayerId = id_type<LayerModel> (getNextId());
    m_processData = constraint.process(m_sharedProcessModelId)->makeViewModelConstructionData();
}

void AddLayerInNewSlot::undo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    auto rack = constraint.rack(m_createdRackId);

    // Removing the slot is enough
    rack->removeSlot(m_createdSlotId);

    // Remove the rack
    if(!m_existingRack)
    {
        constraint.removeRack(m_createdRackId);
    }
}

void AddLayerInNewSlot::redo()
{
    auto& constraint = m_path.find<ConstraintModel>();

    // Rack
    if(!m_existingRack)
    {
        // TODO refactor with AddRackToConstraint
        auto rack = new RackModel{m_createdRackId, &constraint};
        constraint.addRack(rack);
        rack->metadata.setName(QString{"Rack.%1"}.arg(constraint.racks().size()));

        // If it is the first rack created,
        // it is also assigned to all the views of the constraint.
        if(constraint.racks().size() == 1)
        {
            for(const auto& vm : constraint.viewModels())
            {
                vm->showRack(m_createdRackId);
            }
        }
    }

    // Slot
    auto rack = constraint.rack(m_createdRackId);
    rack->addSlot(new SlotModel {m_createdSlotId,
                                rack});

    // Process View
    auto slot = rack->slot(m_createdSlotId);
    auto proc = constraint.process(m_sharedProcessModelId);

    slot->addLayerModel(proc->makeLayer(m_createdLayerId, m_processData, slot));
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
