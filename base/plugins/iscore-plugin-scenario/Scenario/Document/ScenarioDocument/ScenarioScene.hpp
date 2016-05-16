#pragma once
#include <QGraphicsScene>

namespace Scenario
{
class ScenarioScene final : public QGraphicsScene
{
        Q_OBJECT
    public:
        ScenarioScene(QWidget* parent):
            QGraphicsScene{parent}
        {
            setItemIndexMethod(QGraphicsScene::NoIndex);
        }

    signals:
        void pressed(QPointF);
        void moved(QPointF);
        void released(QPointF);

    private:
        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

};
}
