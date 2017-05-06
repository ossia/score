#pragma once
#include <Scenario/Document/Constraint/Slot.hpp>
#include <Scenario/Document/Constraint/ConstraintPresenter.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>

#include <Scenario/Document/Constraint/FullView/FullViewConstraintView.hpp>

namespace Scenario
{
class SlotView;
class SlotHandle;
class ISCORE_PLUGIN_SCENARIO_EXPORT FullViewConstraintPresenter final
    : public ConstraintPresenter
{
  Q_OBJECT

public:
  struct SlotPresenter
  {
    SlotHandle* handle{};
    LayerData process;
  };
  using view_type = FullViewConstraintView;

  FullViewConstraintPresenter(
      const ConstraintModel& viewModel,
      const Process::ProcessPresenterContext& ctx,
      QGraphicsItem* parentobject,
      QObject* parent);

  virtual ~FullViewConstraintPresenter();

  void updateHeight();
  void on_zoomRatioChanged(ZoomRatio val) override;
  int indexOfSlot(const Process::LayerPresenter&);

  const std::vector<SlotPresenter>& getSlots() const {
    return m_slots;
  }
signals:
  void constraintSelected(ConstraintModel&);

private:
  void updateScaling() override;
  void on_defaultDurationChanged(const TimeVal&);
  void on_guiDurationChanged(const TimeVal&);

  void createSlot(int pos, const FullSlot& slt);
  void updateProcessShape(int slot);
  void on_slotRemoved(int);

  void updateProcessesShape();
  void updatePositions();

  double rackHeight() const;
  void on_rackChanged();


  std::vector<SlotPresenter> m_slots;
};
}
