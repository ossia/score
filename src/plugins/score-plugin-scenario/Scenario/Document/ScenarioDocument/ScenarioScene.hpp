#pragma once
#include <QGraphicsScene>

#include <verdigris>

namespace Scenario
{
class ScenarioScene final : public QGraphicsScene
{
  W_OBJECT(ScenarioScene)
public:
  ScenarioScene(QObject* parent);
};
}
