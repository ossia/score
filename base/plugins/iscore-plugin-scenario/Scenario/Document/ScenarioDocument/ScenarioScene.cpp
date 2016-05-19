#include "ScenarioScene.hpp"
#include <QWidget>
namespace Scenario
{

ScenarioScene::ScenarioScene(QWidget* parent):
    QGraphicsScene{parent}
{
    setItemIndexMethod(QGraphicsScene::NoIndex);
}

}
