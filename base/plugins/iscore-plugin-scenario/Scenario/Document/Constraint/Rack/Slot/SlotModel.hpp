#pragma once
#include <Process/LayerModel.hpp>
#include <QObject>
#include <QtGlobal>
#include <iscore/model/Entity.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <nano_signal_slot.hpp>

#include <QString>
#include <functional>

#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>
class DataStream;
class JSONObject;
namespace Process
{
class ProcessModel;
}

namespace Scenario
{
class ConstraintModel;
class RackModel;
// Note : the SlotModel is assumed to be in a Rack, itself in a Constraint.
class ISCORE_PLUGIN_SCENARIO_EXPORT SlotModel final
    : public iscore::Entity<SlotModel>,
      public Nano::Observer
{
  Q_OBJECT
  ISCORE_SERIALIZE_FRIENDS

  Q_PROPERTY(
      qreal getHeight READ getHeight WRITE setHeight NOTIFY HeightChanged)

  Q_PROPERTY(bool focus READ focus WRITE setFocus NOTIFY focusChanged)

public:
  SlotModel(
      const Id<SlotModel>& id, const qreal slotHeight, RackModel* parent);

  // Copy
  SlotModel(
      const SlotModel& source,
      const Id<SlotModel>& id,
      RackModel* parent);

  RackModel& rack() const;

  static void copyViewModelsInSameConstraint(const SlotModel&, SlotModel&);

  template <typename Impl>
  SlotModel(Impl& vis, QObject* parent) : Entity{vis, parent}
  {
    vis.writeTo(*this);
  }

  virtual ~SlotModel() = default;

  // A process is selected for edition when it is
  // the edited process when the interface is clicked.
  void putToFront(const OptionalId<Process::ProcessModel>& layerId);
  const Process::ProcessModel* frontLayerModel() const;

  // A slot is always in a constraint
  ConstraintModel& parentConstraint() const;

  qreal getHeight() const;
  bool focus() const;

  void
  on_deleteSharedProcessModel(const Process::ProcessModel& sharedProcessId);

  void setHeight(qreal arg);
  void setFocus(bool arg);

  void addLayer(Id<Process::ProcessModel> p);
  void removeLayer(Id<Process::ProcessModel> p);
  const std::vector<Id<Process::ProcessModel>>& layers() const { return m_processes; }

signals:
  void layerModelPutToFront(const Process::ProcessModel& layerModelId);

  void HeightChanged(qreal arg);
  void focusChanged(bool arg);

  void layerAdded(const Process::ProcessModel&);
  void layerRemoved(const Process::ProcessModel&);

private:
  OptionalId<Process::ProcessModel> m_frontLayerModelId;
  std::vector<Id<Process::ProcessModel>> m_processes;

  qreal m_height{200};
  bool m_focus{false};
};
ISCORE_PARAMETER_TYPE(SlotModel, Height)

/**
 * @brief parentConstraint Utility function to get the parent constraint of a
 * process view model
 * @param lm Process view model pointer
 *
 * @return A pointer to the parent constraint if there is one, or nullptr.
 */
ConstraintModel* parentConstraint(Process::ProcessModel* lm);
}

DEFAULT_MODEL_METADATA(Scenario::SlotModel, "Slot")
TR_TEXT_METADATA(, Scenario::SlotModel, PrettyName_k, "Slot")
