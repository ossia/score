#include "BoxModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Slot/SlotModel.hpp"


BoxModel::BoxModel(const id_type<BoxModel>& id, QObject* parent) :
    IdentifiedObject<BoxModel> {id, "BoxModel", parent}
{

}

BoxModel::BoxModel(const BoxModel& source,
                   const id_type<BoxModel>& id,
                   std::function<void(const SlotModel&, SlotModel&)> lmCopyMethod,
                   QObject *parent) :
    IdentifiedObject<BoxModel> {id, "BoxModel", parent}
{
    metadata = source.metadata;
    for(auto& slot : source.m_slots)
    {
        addSlot(new SlotModel{lmCopyMethod, *slot, slot->id(), this},
                source.slotPosition(slot->id()));
    }
}



ConstraintModel& BoxModel::constraint() const
{
    return static_cast<ConstraintModel&>(*this->parent());
}

void BoxModel::addSlot(SlotModel* slot, int position)
{
    // Connection
    connect(this, &BoxModel::on_deleteSharedProcessModel,
            slot, &SlotModel::on_deleteSharedProcessModel);
    m_slots.insert(slot);
    m_positions.insert(position, slot->id());

    emit slotCreated(slot->id());
    emit slotPositionsChanged();
}

void BoxModel::addSlot(SlotModel* m)
{
    addSlot(m, m_positions.size());
}


void BoxModel::removeSlot(const id_type<SlotModel>& slotId)
{
    auto removedSlot = slot(slotId);

    // Make the remaining slots decrease their position.
    m_positions.removeAll(slotId);
    m_slots.remove(slotId);

    emit slotRemoved(slotId);
    emit slotPositionsChanged();
    delete removedSlot;
}

void BoxModel::swapSlots(const id_type<SlotModel>& firstslot,
                         const id_type<SlotModel>& secondslot)
{
    m_positions.swap(m_positions.indexOf(firstslot), m_positions.indexOf(secondslot));
    emit slotPositionsChanged();
}

SlotModel* BoxModel::slot(const id_type<SlotModel>& slotId) const
{
    return m_slots.at(slotId);
}
