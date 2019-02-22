#pragma once
#include <QGraphicsScene>

#include <wobjectdefs.h>

namespace Scenario
{
class ScenarioScene final : public QGraphicsScene
{
  W_OBJECT(ScenarioScene)
public:
  ScenarioScene(QObject* parent);
};
}
