#pragma once
#include <QString>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <nano_signal_slot.hpp>

#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>

class QObject;
namespace Scenario
{
class RackModel;
class ConstraintModel;
class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintViewModel
    : public IdentifiedObject<ConstraintViewModel>,
      public Nano::Observer
{
  Q_OBJECT

public:
  ConstraintViewModel(
      const Id<ConstraintViewModel>& id,
      const QString& name,
      ConstraintModel& model,
      QObject* parent);

  virtual ~ConstraintViewModel();

  template <typename DeserializerVisitor>
  ConstraintViewModel(
      DeserializerVisitor&& vis, ConstraintModel& model, QObject* parent)
      : IdentifiedObject{vis, parent}, m_model{model}
  {
    vis.writeTo(*this);
  }

  virtual ConstraintViewModel* clone(
      const Id<ConstraintViewModel>& id, ConstraintModel& cm, QObject* parent)
      = 0;

  virtual QString type() const = 0;
  ConstraintModel& model() const;

  bool isRackShown() const;
  const OptionalId<RackModel>& shownRack() const;

  void hideRack();
  void showRack(const OptionalId<RackModel>& rackId);

  virtual void on_rackRemoval(const RackModel&);

signals:
  void lastRackRemoved();
  void rackHidden();
  void rackShown(const OptionalId<RackModel>&);

  void aboutToBeDeleted(ConstraintViewModel*);

private:
  // A view model cannot be constructed without a model
  // hence we are safe with a pointer
  ConstraintModel& m_model;

  OptionalId<RackModel> m_shownRack{};
};
}
