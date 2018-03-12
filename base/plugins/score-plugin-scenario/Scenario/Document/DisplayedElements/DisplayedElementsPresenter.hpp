#pragma once
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include <QObject>
#include <Scenario/Document/BaseScenario/BaseScenarioPresenter.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>

#include <vector>

namespace Process
{
class LayerPresenter;
}

class BaseGraphicsObject;
namespace Scenario
{
class FullViewIntervalPresenter;
class ScenarioDocumentPresenter;
class IntervalModel;
// Contains the elements that are shown (not necessarily the ones in
// BaseScenarioModel)
class SCORE_PLUGIN_SCENARIO_EXPORT DisplayedElementsPresenter final
    : public QObject,
      public BaseScenarioPresenter<DisplayedElementsModel, FullViewIntervalPresenter>
{
  Q_OBJECT
public:
  DisplayedElementsPresenter(ScenarioDocumentPresenter& parent);
  ~DisplayedElementsPresenter();
  using QObject::event;
  using BaseScenarioPresenter<DisplayedElementsModel, FullViewIntervalPresenter>::
      event;

  BaseGraphicsObject& view() const;

  void on_displayedIntervalChanged(const IntervalModel& m);
  void showInterval();

  void on_zoomRatioChanged(ZoomRatio r);

  void on_displayedIntervalDurationChanged(TimeVal);
  void on_displayedIntervalHeightChanged(double);

Q_SIGNALS:
  void requestFocusedPresenterChange(Process::LayerPresenter*);

private:
  void on_intervalExecutionTimer();
  void updateLength(double);

  ScenarioDocumentPresenter& m_model;

  std::vector<QMetaObject::Connection> m_connections;
};
}
