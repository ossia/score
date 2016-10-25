#include <iscore/model/ModelMetadata.hpp>
#include "RackModel.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <iscore/tools/EntityMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>

namespace Scenario
{
RackModel::RackModel(const Id<RackModel>& id, QObject* parent) :
    Entity{id, Metadata<ObjectKey_k, RackModel>::get(), parent}
{
    initConnections();
    metadata().setInstanceName(*this);
}

RackModel::RackModel(const RackModel& source,
                   const Id<RackModel>& id,
                   std::function<void(const SlotModel&, SlotModel&)> lmCopyMethod,
                   QObject *parent) :
    Entity{source, id, Metadata<ObjectKey_k, RackModel>::get(), parent}
{
    metadata().setInstanceName(*this);
    initConnections();
    for(const auto& slot : source.slotmodels)
    {
        auto new_slot = new SlotModel{lmCopyMethod, slot, Id<SlotModel>{slot.id_val()}, this};
        addSlot(new_slot,
                source.slotPosition(new_slot->id()));
    }
}



ConstraintModel& RackModel::constraint() const
{
    return safe_cast<ConstraintModel&>(*this->parent());
}

void RackModel::addSlot(SlotModel* slot, int position)
{
    // Connection
    connect(this, &RackModel::on_deleteSharedProcessModel,
            slot, &SlotModel::on_deleteSharedProcessModel);

    m_positions.insert(position, slot->id());
    slotmodels.add(slot);

    emit slotPositionsChanged();
}

void RackModel::addSlot(SlotModel* m)
{
    addSlot(m, m_positions.size());
}

void RackModel::on_slotRemoved(const SlotModel& slot)
{
    // Make the remaining slots decrease their position.
    m_positions.removeAll(slot.id());

    emit slotPositionsChanged();
}

void RackModel::swapSlots(const Id<SlotModel>& firstslot,
                         const Id<SlotModel>& secondslot)
{
    m_positions.swap(m_positions.indexOf(firstslot), m_positions.indexOf(secondslot));
    emit slotPositionsChanged();
}

void RackModel::initConnections()
{
    slotmodels.removing.connect<RackModel, &RackModel::on_slotRemoved>(this);
}
}
