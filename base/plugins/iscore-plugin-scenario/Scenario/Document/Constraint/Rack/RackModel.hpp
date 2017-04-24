#pragma once
#include <Process/TimeValue.hpp>
#include <QList>
#include <QObject>
#include <iscore/model/Entity.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <nano_signal_slot.hpp>

#include <QString>
#include <functional>

#include "Slot/SlotModel.hpp"
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore_plugin_scenario_export.h>
namespace Process
{
class ProcessModel;
}

namespace Scenario
{
class ConstraintModel;
/**
 * @brief The RackModel class
 *
 * A Rack is a slot container.
 * A Rack is always found in a Constraint.
 */
class ISCORE_PLUGIN_SCENARIO_EXPORT RackModel final
    : public iscore::Entity<RackModel>,
      public Nano::Observer
{
  Q_OBJECT

public:
  RackModel(const Id<RackModel>& id, QObject* parent);

  // Copy
  RackModel(
      const RackModel& source,
      const Id<RackModel>& id,
      QObject* parent);

  template <typename Impl>
  RackModel(Impl& vis, QObject* parent) : Entity{vis, parent}
  {
    initConnections();
    vis.writeTo(*this);
  }

  // A rack is necessarily child of a constraint.
  ConstraintModel& constraint() const;

  void addSlot(SlotModel* m, int position);
  void addSlot(SlotModel* m); // No position : at the end

  void
  swapSlots(const Id<SlotModel>& firstslot, const Id<SlotModel>& secondslot);
  int slotPosition(const Id<SlotModel>& slotId) const;

  iscore::EntityMap<SlotModel> slotmodels;
signals:
  void slotPositionsChanged();

  void on_durationChanged(const TimeVal&);

private:
  void initConnections();
  void on_slotRemoved(const SlotModel&);
};
}
DEFAULT_MODEL_METADATA(Scenario::RackModel, "Rack")
Q_DECLARE_METATYPE(Id<Scenario::RackModel>)
TR_TEXT_METADATA(, Scenario::RackModel, PrettyName_k, "Rack")
