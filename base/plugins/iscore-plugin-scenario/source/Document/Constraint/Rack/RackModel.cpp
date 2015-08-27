#include "RackModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Slot/SlotModel.hpp"


RackModel::RackModel(const Id<RackModel>& id, QObject* parent) :
    IdentifiedObject<RackModel> {id, "RackModel", parent}
{

}

RackModel::RackModel(const RackModel& source,
                   const Id<RackModel>& id,
                   std::function<void(const SlotModel&, SlotModel&)> lmCopyMethod,
                   QObject *parent) :
    IdentifiedObject<RackModel> {id, "RackModel", parent}
{
    for(const auto& slot : source.m_slots)
    {
        addSlot(new SlotModel{lmCopyMethod, slot, slot.id(), this},
                source.slotPosition(slot.id()));
    }
}



ConstraintModel& RackModel::constraint() const
{
    return static_cast<ConstraintModel&>(*this->parent());
}

void RackModel::addSlot(SlotModel* slot, int position)
{
    // Connection
    connect(this, &RackModel::on_deleteSharedProcessModel,
            slot, &SlotModel::on_deleteSharedProcessModel);
    m_slots.insert(slot);
    m_positions.insert(position, slot->id());

    emit slotCreated(slot->id());
    emit slotPositionsChanged();
}

void RackModel::addSlot(SlotModel* m)
{
    addSlot(m, m_positions.size());
}


void RackModel::removeSlot(const Id<SlotModel>& slotId)
{
    auto& removedSlot = slot(slotId);

    // Make the remaining slots decrease their position.
    m_positions.removeAll(slotId);
    m_slots.remove(slotId);

    emit slotRemoved(slotId);
    emit slotPositionsChanged();
    delete &removedSlot;
}

void RackModel::swapSlots(const Id<SlotModel>& firstslot,
                         const Id<SlotModel>& secondslot)
{
    m_positions.swap(m_positions.indexOf(firstslot), m_positions.indexOf(secondslot));
    emit slotPositionsChanged();
}

SlotModel& RackModel::slot(const Id<SlotModel>& slotId) const
{
    return m_slots.at(slotId);
}
