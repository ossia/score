#include "AddProcessToConstraint.hpp"
#include "AddLayerInNewSlot.hpp"
#include "Rack/Slot/AddLayerModelToSlot.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"

#include "ProcessInterface/ProcessModel.hpp"

#include "ProcessInterface/ProcessList.hpp"
#include "ProcessInterface/ProcessFactory.hpp"

#include "iscore/document/DocumentInterface.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"
using namespace iscore;
using namespace Scenario::Command;
AddProcessToConstraint::AddProcessToConstraint(ObjectPath&& constraintPath, QString process) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {std::move(constraintPath) },
    m_processName {process}
{
    auto& constraint = m_path.find<ConstraintModel>();
    m_createdProcessId = getStrongId(constraint.processes());
    m_noRackes = (constraint.racks().empty() && constraint.objectName() != "BaseConstraintModel" );
    m_notBaseConstraint = (constraint.objectName() != "BaseConstraintModel");

    if(m_noRackes)
    {
        m_createdRackId = getStrongId(constraint.racks());
        m_createdSlotId = id_type<SlotModel> (getNextId());
        m_createdLayerId = id_type<LayerModel> (getNextId());
        m_layerConstructionData = ProcessList::getFactory(m_processName)->makeStaticLayerConstructionData();
    }
    else if (m_notBaseConstraint)
    {
        // TODO what if there is a rack without slots???
        const auto& firstSlotModel = *(*constraint.racks().begin()).getSlots().begin();

        m_layerConstructionData = ProcessList::getFactory(m_processName)->makeStaticLayerConstructionData();
        m_createdLayerId = getStrongId(firstSlotModel.layerModels());
    }
    else
    {
        // Base constraint : add in new slot?
    }
}

void AddProcessToConstraint::undo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    if(m_noRackes)
    {
        auto& rack = constraint.rack(m_createdRackId);

        // Removing the slot will remove the layer
        rack.removeSlot(m_createdSlotId);
        constraint.removeRack(m_createdRackId);
    }
    else if(m_notBaseConstraint)
    {
        auto& slot = *(*constraint.racks().begin()).getSlots().begin();
        slot.deleteLayerModel(m_createdLayerId);
    }

    constraint.removeProcess(m_createdProcessId);
}

void AddProcessToConstraint::redo()
{
    auto& constraint = m_path.find<ConstraintModel>();

    // Create process model
    auto proc = ProcessList::getFactory(m_processName)
            ->makeModel(
                constraint.defaultDuration(),
                m_createdProcessId,
                &constraint);

    constraint.addProcess(proc);
    if(m_noRackes)
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

        // Slot
        rack->addSlot(new SlotModel {m_createdSlotId,
                                    rack});

        // Process View
        auto& slot = rack->slot(m_createdSlotId);

        slot.addLayerModel(proc->makeLayer(m_createdLayerId, m_layerConstructionData, &slot));
    }
    else if(m_notBaseConstraint)
    {
        auto& slot = *(*constraint.racks().begin()).getSlots().begin();

        slot.addLayerModel(proc->makeLayer(m_createdLayerId, m_layerConstructionData, &slot));
    }
}

void AddProcessToConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path
      << m_processName
      << m_createdProcessId
      << m_createdRackId
      << m_createdSlotId
      << m_createdLayerId
      << m_layerConstructionData
      << m_noRackes
      << m_notBaseConstraint;
}

void AddProcessToConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path
      >> m_processName
      >> m_createdProcessId
      >> m_createdRackId
      >> m_createdSlotId
      >> m_createdLayerId
      >> m_layerConstructionData
      >> m_noRackes
      >> m_notBaseConstraint;
}
