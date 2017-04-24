#include "RackModel.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/Todo.hpp>

namespace Scenario
{
RackModel::RackModel(const Id<RackModel>& id, QObject* parent)
    : Entity{id, Metadata<ObjectKey_k, RackModel>::get(), parent}
{
  initConnections();
  metadata().setInstanceName(*this);
}

RackModel::RackModel(
    const RackModel& source,
    const Id<RackModel>& id,
    QObject* parent)
    : Entity{source, id, Metadata<ObjectKey_k, RackModel>::get(), parent}
{
  metadata().setInstanceName(*this);
  initConnections();
  for (const auto& slot : source.slotmodels)
  {
    auto new_slot = new SlotModel{slot, Id<SlotModel>{slot.id_val()}, this};
    addSlot(new_slot, source.slotPosition(new_slot->id()));
  }
}

ConstraintModel& RackModel::constraint() const
{
  return safe_cast<ConstraintModel&>(*this->parent());
}

void RackModel::addSlot(SlotModel* slot, int position)
{
  // Connection
  connect(
      this, &RackModel::on_deleteProcess, slot,
      &SlotModel::on_deleteSharedProcessModel);

  auto& map = slotmodels.unsafe_map();
  auto& ordered = map.ordered();
  auto it = ordered.begin();
  std::advance(it, position);
  ordered.insert(it, slot);

  slotmodels.mutable_added(*slot);
  slotmodels.added(*slot);

  emit slotPositionsChanged();
}

void RackModel::addSlot(SlotModel* m)
{
  addSlot(m, slotmodels.size());
}

void RackModel::on_slotRemoved(const SlotModel& slot)
{
  emit slotPositionsChanged();
}

void RackModel::swapSlots(
    const Id<SlotModel>& firstslot, const Id<SlotModel>& secondslot)
{
  slotmodels.swap(firstslot, secondslot);
  emit slotPositionsChanged();
}

int RackModel::slotPosition(const Id<SlotModel>& slotId) const
{
  int i = 0;
  for(auto& e : slotmodels)
    if(e.id() == slotId)
      return i;
    else
      i++;

  return -1; // To follow QList::indexOf.
}

void RackModel::initConnections()
{
  slotmodels.removing.connect<RackModel, &RackModel::on_slotRemoved>(this);
}
}
