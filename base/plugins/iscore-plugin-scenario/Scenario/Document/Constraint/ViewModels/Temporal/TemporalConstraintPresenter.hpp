#pragma once
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>

#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <iscore_plugin_scenario_export.h>
class QGraphicsItem;
class QObject;
namespace Scenario
{
class ISCORE_PLUGIN_SCENARIO_EXPORT TemporalConstraintPresenter final
    : public ConstraintPresenter
{
  Q_OBJECT

public:
  using viewmodel_type = TemporalConstraintViewModel;
  using view_type = TemporalConstraintView;
  const auto& id() const
  {
    return ConstraintPresenter::id();
  } // To please boost::const_mem_fun

  TemporalConstraintPresenter(
      const TemporalConstraintViewModel& viewModel,
      const Process::ProcessPresenterContext& ctx,
      QGraphicsItem* parentobject,
      QObject* parent);

  virtual ~TemporalConstraintPresenter();

  void on_requestOverlayMenu(QPointF);


  RackPresenter* rack() const;
  void on_rackShown(const OptionalId<RackModel>&);
  void on_rackHidden();
  void on_noRacks();

  void on_racksChanged(const RackModel&);
  void on_racksChanged();

  void updateScaling() override;
  void on_zoomRatioChanged(ZoomRatio val) override;
  void on_defaultDurationChanged(const TimeVal&) override;

  void updateHeight();

signals:
  void constraintHoverEnter();
  void constraintHoverLeave();

private:
  void createRackPresenter(const RackModel&);
  void clearRackPresenter();
  void on_rackRemoved(const RackModel&);

  RackPresenter* m_rack{};
};
}
