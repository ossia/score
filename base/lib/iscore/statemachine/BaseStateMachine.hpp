#pragma once
#include <QStateMachine>
#include <QPointF>

class QGraphicsScene;
class BaseStateMachine : public QStateMachine
{
    public:
        explicit BaseStateMachine(const QGraphicsScene& scene);
        virtual ~BaseStateMachine();

        QPointF scenePoint;

        const QGraphicsScene& scene() const;

    private:
        const QGraphicsScene& m_scene;
};
