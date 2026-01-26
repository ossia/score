#pragma once

#include <Process/TimeValue.hpp>

#include <QPoint>
#include <QTimer>

class QGraphicsItem;
namespace Scenario
{
class ToolPalette;
class ScenarioExecution;
struct Point;

class PlayToolState
{
public:
  explicit PlayToolState(const Scenario::ToolPalette& sm);

  void on_pressed(QPointF scenePoint, Scenario::Point scenarioPoint);
  void on_moved(QPointF scenePoint, Scenario::Point scenarioPoint);
  void on_released(QPointF scenePoint, Scenario::Point scenarioPoint);

private:
  void on_scrub(QPointF scenePoint, Scenario::Point scenarioPoint);
  const Scenario::ToolPalette& m_sm;
  ScenarioExecution& m_exec;

  QGraphicsItem* m_pressedItem{};

  TimeVal m_previousPoint;
  TimeVal m_targetPosition;
  double m_previousSpeed{1.};
  double m_smoothedSpeed = 1.0;
  double m_maxSpeed{1.0};

  QTimer m_scrubbingTimer;
  bool m_speedChanged{};
};
}
