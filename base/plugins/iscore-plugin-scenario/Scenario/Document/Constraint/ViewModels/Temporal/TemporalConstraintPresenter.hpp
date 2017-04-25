#pragma once
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>

#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <iscore_plugin_scenario_export.h>
class QGraphicsItem;
class QObject;
namespace Process
{
class ProcessModel;
}
namespace Scenario
{
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
      QGraphicsItem* parentobject,
      QObject* parent);

  virtual ~TemporalConstraintPresenter();

  void on_requestOverlayMenu(QPointF);


  void on_rackVisibleChanged(bool);

  void updateScaling() override;
  void on_zoomRatioChanged(ZoomRatio val) override;
  void on_defaultDurationChanged(const TimeVal&) override;

  void updateHeight();

signals:
  void constraintHoverEnter();
  void constraintHoverLeave();

private:
  void on_processesChanged(const Process::ProcessModel&);
  //void createRackPresenter(const RackModel&);
  void clearRackPresenter();
  //void on_rackRemoved(const RackModel&);
  double rackHeight() const;
};

}
