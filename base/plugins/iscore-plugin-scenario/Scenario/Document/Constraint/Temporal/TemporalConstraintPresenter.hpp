#pragma once
#include <Scenario/Document/Constraint/ConstraintPresenter.hpp>

#include <Scenario/Document/Constraint/Slot.hpp>
#include <Scenario/Document/Constraint/Temporal/TemporalConstraintView.hpp>
#include <iscore_plugin_scenario_export.h>
class QGraphicsItem;
class QObject;
namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class SlotHandle;
class SlotHeader;
class TemporalConstraintHeader;
class ISCORE_PLUGIN_SCENARIO_EXPORT TemporalConstraintPresenter final
    : public ConstraintPresenter
{
  Q_OBJECT

public:
  using view_type = TemporalConstraintView;
  const auto& id() const
  {
    return ConstraintPresenter::id();
  } // To please boost::const_mem_fun

  TemporalConstraintPresenter(
      const ConstraintModel& viewModel,
      const Process::ProcessPresenterContext& ctx,
      bool handles,
      QGraphicsItem* parentobject,
      QObject* parent);

  virtual ~TemporalConstraintPresenter();

  void updateScaling() override;
  void updateHeight();

  int indexOfSlot(const Process::LayerPresenter&);
  void on_zoomRatioChanged(ZoomRatio val) override;

  void changeRackState();
  void selectedSlot(int) const override;
  TemporalConstraintView* view() const;
  TemporalConstraintHeader* header() const;
signals:
  void constraintHoverEnter();
  void constraintHoverLeave();

private:
  double rackHeight() const;
  void createSlot(int pos, const Slot& slt);
  void createLayer(int slot, const Process::ProcessModel& proc);
  void updateProcessShape(int slot, const LayerData& data);
  void removeLayer(const Process::ProcessModel& proc);
  void on_slotRemoved(int pos);
  void updateProcessesShape();
  void updatePositions();
  void on_layerModelPutToFront(int slot, const Process::ProcessModel& proc);
  void on_layerModelPutToBack(int slot, const Process::ProcessModel& proc);
  void on_rackChanged();
  void on_processesChanged(const Process::ProcessModel&);
  void on_requestOverlayMenu(QPointF);
  void on_rackVisibleChanged(bool);
  void on_defaultDurationChanged(const TimeVal&);

  struct SlotPresenter
  {
    SlotHeader* header{};
    SlotHandle* handle{};
    std::vector<LayerData> processes;
  };

  std::vector<SlotPresenter> m_slots;
  bool m_handles{true};
};

}
