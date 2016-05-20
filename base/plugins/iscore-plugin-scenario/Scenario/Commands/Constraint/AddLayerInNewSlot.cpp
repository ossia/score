#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <algorithm>
#include <vector>

#include "AddLayerInNewSlot.hpp"
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Settings/Model.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/application/ApplicationContext.hpp>

namespace Scenario
{
namespace Command
{
AddLayerInNewSlot::AddLayerInNewSlot(
        Path<ConstraintModel>&& constraintPath,
        Id<Process::ProcessModel> process) :
    m_path {std::move(constraintPath) },
    m_sharedProcessModelId{std::move(process)}
{
    auto& constraint = m_path.find();

    if(constraint.racks.empty())
    {
        m_createdRackId = getStrongId(constraint.racks);
        m_existingRack = false;
        m_createdSlotId = Id<SlotModel> (iscore::id_generator::getFirstId());
    }
    else
    {
        for(auto vm : constraint.viewModels())
        {
            m_createdRackId = vm->shownRack();
        }
        auto& rack = constraint.racks.at(m_createdRackId);
        m_existingRack = true;
        m_createdSlotId = getStrongId(rack.slotmodels);
    }

    m_createdLayerId = Id<Process::LayerModel> (iscore::id_generator::getFirstId());
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

    auto h = iscore::AppContext().settings<Scenario::Settings::Model>().getSlotHeight();
    // Slot
    auto& rack = constraint.racks.at(m_createdRackId);
    rack.addSlot(new SlotModel {m_createdSlotId,
                                h,
                                &rack});

    // Process View
    auto& slot = rack.slotmodels.at(m_createdSlotId);
    auto& proc = constraint.processes.at(m_sharedProcessModelId);

    slot.layers.add(proc.makeLayer(m_createdLayerId,
                                      m_processData,
                                      &slot));
}

void AddLayerInNewSlot::serializeImpl(DataStreamInput& s) const
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

void AddLayerInNewSlot::deserializeImpl(DataStreamOutput& s)
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
}
}
