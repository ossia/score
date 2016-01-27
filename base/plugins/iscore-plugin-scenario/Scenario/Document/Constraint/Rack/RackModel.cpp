#include <Process/ModelMetadata.hpp>
#include "RackModel.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>

namespace Scenario
{
RackModel::RackModel(const Id<RackModel>& id, QObject* parent) :
    IdentifiedObject<RackModel> {id, Metadata<ObjectKey_k, RackModel>::get(), parent}
{
    initConnections();
    metadata.setName(QString{"Rack.%1"} .arg(*id.val()));
}

RackModel::RackModel(const RackModel& source,
                   const Id<RackModel>& id,
                   std::function<void(const SlotModel&, SlotModel&)> lmCopyMethod,
                   QObject *parent) :
    IdentifiedObject<RackModel> {id, Metadata<ObjectKey_k, RackModel>::get(), parent}
{
    initConnections();
    for(const auto& slot : source.slotmodels)
    {
        addSlot(new SlotModel{lmCopyMethod, slot, slot.id(), this},
                source.slotPosition(slot.id()));
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
