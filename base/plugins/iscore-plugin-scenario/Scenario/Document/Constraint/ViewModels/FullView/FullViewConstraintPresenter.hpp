#pragma once
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>

#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>

namespace Scenario
{
class SlotView;
class SlotHandle;
class ISCORE_PLUGIN_SCENARIO_EXPORT FullViewConstraintPresenter final
    : public ConstraintPresenter
{
  Q_OBJECT

public:
  using view_type = FullViewConstraintView;

  FullViewConstraintPresenter(
      const ConstraintModel& viewModel,
      const Process::ProcessPresenterContext& ctx,
      QGraphicsItem* parentobject,
      QObject* parent);

  virtual ~FullViewConstraintPresenter();

  void updateHeight();
  void on_zoomRatioChanged(ZoomRatio val) override;

signals:
  void constraintSelected(ConstraintModel&);

private:
  void updateScaling() override;
  void on_defaultDurationChanged(const TimeVal&) override;

  void createSlot(int pos, const FullSlot& slt);
  void updateProcessShape(int slot);
  void on_slotRemoved(int);

  void updateProcessesShape();
  void updatePositions();

  double rackHeight() const;

  struct SlotPresenter
  {
    SlotView* view{};
    SlotHandle* handle{};
    LayerData process;
  };

  std::vector<SlotPresenter> m_slots;
};
}
