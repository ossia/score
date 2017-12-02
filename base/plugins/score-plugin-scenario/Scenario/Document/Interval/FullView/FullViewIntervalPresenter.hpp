#pragma once
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/SlotPresenter.hpp>
#include <score/selection/SelectionDispatcher.hpp>

#include <Scenario/Document/Interval/FullView/FullViewIntervalView.hpp>

namespace Scenario
{
class SlotView;
class SlotHandle;
class SCORE_PLUGIN_SCENARIO_EXPORT FullViewIntervalPresenter final
    : public IntervalPresenter
{
  Q_OBJECT

public:
  using view_type = FullViewIntervalView;

  FullViewIntervalPresenter(
      const IntervalModel& viewModel,
      const Process::ProcessPresenterContext& ctx,
      QGraphicsItem* parentobject,
      QObject* parent);

  virtual ~FullViewIntervalPresenter();

  void updateHeight();
  void on_zoomRatioChanged(ZoomRatio val) override;
  int indexOfSlot(const Process::LayerPresenter&);

  const std::vector<SlotPresenter>& getSlots() const {
    return m_slots;
  }
signals:
  void intervalSelected(IntervalModel&);

private:
  void updateScaling() override;
  void on_defaultDurationChanged(const TimeVal&);
  void on_guiDurationChanged(const TimeVal&);

  void createSlot(int pos, const FullSlot& slt);
  void updateProcessShape(int slot);
  void updateProcessShape(const LayerData& layer);
  void on_slotRemoved(int);

  void updateProcessesShape();
  void updatePositions();

  double rackHeight() const;
  void on_rackChanged();


  std::vector<SlotPresenter> m_slots;
};
}
