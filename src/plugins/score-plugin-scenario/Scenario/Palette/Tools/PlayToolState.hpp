#pragma once

#include <QPoint>

namespace Scenario
{
class ToolPalette;
class ScenarioExecution;
struct Point;

class PlayToolState
{
public:
  PlayToolState(const Scenario::ToolPalette& sm);

  void on_pressed(QPointF scenePoint, Scenario::Point scenarioPoint);
  void on_moved(QPointF scenePoint, Scenario::Point scenarioPoint);
  void on_released(QPointF scenePoint, Scenario::Point scenarioPoint);

private:
  const Scenario::ToolPalette& m_sm;
  ScenarioExecution& m_exec;

  QGraphicsItem* m_pressedItem{};
};
}
