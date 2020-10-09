#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioPresenter.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>

#include <QObject>

#include <vector>
#include <verdigris>

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
  W_OBJECT(DisplayedElementsPresenter)
public:
  DisplayedElementsPresenter(ScenarioDocumentPresenter& parent);
  ~DisplayedElementsPresenter();
  using QObject::event;
  using BaseScenarioPresenter<DisplayedElementsModel, FullViewIntervalPresenter>::event;

  BaseGraphicsObject& view() const;

  void on_displayedIntervalChanged(const IntervalModel& m);
  void showInterval();

  void on_zoomRatioChanged(ZoomRatio r);

  void on_displayedIntervalDurationChanged(TimeVal);
  void on_displayedIntervalHeightChanged(double);

  void recomputeHeight();

  void setVisible(bool);
  void remove();

  void setSnapLine(TimeVal t, bool enabled);

public:
  void requestFocusedPresenterChange(Process::LayerPresenter* arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, requestFocusedPresenterChange, arg_1)

private:
  void on_intervalExecutionTimer();
  void updateLength(double);

  ScenarioDocumentPresenter& m_model;

  std::vector<QMetaObject::Connection> m_connections;
};
}
