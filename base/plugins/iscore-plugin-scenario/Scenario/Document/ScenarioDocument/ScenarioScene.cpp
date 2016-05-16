#include "ScenarioScene.hpp"

namespace Scenario
{

ScenarioScene::ScenarioScene(QWidget* parent):
    QGraphicsScene{parent}
{
    setItemIndexMethod(QGraphicsScene::NoIndex);
}

}
