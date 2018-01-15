#pragma once
#include <Scenario/Document/Interval/IntervalPresenter.hpp>

#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <score_plugin_scenario_export.h>
class QGraphicsItem;
class QObject;
namespace Process
{
class ProcessModel;
class GraphicsShapeItem;
}
namespace Scenario
{
class SlotHandle;
class SlotHeader;
class DefaultHeaderDelegate;
class TemporalIntervalHeader;
class SCORE_PLUGIN_SCENARIO_EXPORT TemporalIntervalPresenter final
    : public IntervalPresenter
{
  Q_OBJECT

public:
  using view_type = TemporalIntervalView;
  const auto& id() const
  {
    return IntervalPresenter::id();
  } // To please boost::const_mem_fun

  TemporalIntervalPresenter(
      const IntervalModel& viewModel,
      const Process::ProcessPresenterContext& ctx,
      bool handles,
      QGraphicsItem* parentobject,
      QObject* parent);

  virtual ~TemporalIntervalPresenter();

  void updateScaling() override;
  void updateHeight();

  int indexOfSlot(const Process::LayerPresenter&);
  void on_zoomRatioChanged(ZoomRatio val) override;

  void changeRackState();
  void selectedSlot(int) const override;
  TemporalIntervalView* view() const;
  TemporalIntervalHeader* header() const;

  void requestSlotMenu(int slot, QPoint pos, QPointF sp) const override;
Q_SIGNALS:
  void intervalHoverEnter();
  void intervalHoverLeave();

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
    DefaultHeaderDelegate* headerDelegate{};
    SlotHandle* handle{};
    std::vector<LayerData> processes;
  };

  std::vector<SlotPresenter> m_slots;
  bool m_handles{true};
  void setHeaderWidth(const SlotPresenter& slot, double w);
};

}
