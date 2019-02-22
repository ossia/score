#pragma once
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/Interval/SlotPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>

#include <score_plugin_scenario_export.h>
#include <wobjectdefs.h>
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
struct SlotPresenter;
class SCORE_PLUGIN_SCENARIO_EXPORT TemporalIntervalPresenter final
    : public IntervalPresenter
{
  W_OBJECT(TemporalIntervalPresenter)

public:
  using view_type = TemporalIntervalView;
  const auto& id() const
  {
    return IntervalPresenter::id();
  } // To please boost::const_mem_fun

  TemporalIntervalPresenter(
      const IntervalModel& viewModel,
      const Process::ProcessPresenterContext& ctx, bool handles,
      QGraphicsItem* parentobject, QObject* parent);

  ~TemporalIntervalPresenter() override;

  void updateScaling() override;
  void updateHeight();

  int indexOfSlot(const Process::LayerPresenter&);
  void on_zoomRatioChanged(ZoomRatio val) override;

  void changeRackState();
  void selectedSlot(int) const override;
  TemporalIntervalView* view() const;
  TemporalIntervalHeader* header() const;

  void requestSlotMenu(int slot, QPoint pos, QPointF sp) const override;
  void requestProcessSelectorMenu(int slot, QPoint pos, QPointF sp) const;

public:
  void intervalHoverEnter()
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, intervalHoverEnter);
  void intervalHoverLeave()
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, intervalHoverLeave);

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

  void startSlotDrag(int slot, QPointF) const override;
  void stopSlotDrag() const override;

  bool m_handles{true};
  void setHeaderWidth(const SlotPresenter& slot, double w);
};
}
