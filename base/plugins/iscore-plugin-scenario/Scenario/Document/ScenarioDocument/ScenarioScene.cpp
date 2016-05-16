#include "ScenarioScene.hpp"
#include <QGraphicsSceneMouseEvent>

namespace Scenario
{

void ScenarioScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if(event->button() == Qt::MouseButton::LeftButton)
        emit pressed(event->scenePos());
}

void ScenarioScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    emit moved(event->scenePos());
}

void ScenarioScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    emit released(event->scenePos());
}

}
