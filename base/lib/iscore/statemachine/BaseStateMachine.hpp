#pragma once
#include <QStateMachine>
#include <QPointF>

class QGraphicsScene;
/**
 * @brief The BaseStateMachine class
 *
 * Common class to build state machines that work in a QGraphicsScene.
 */
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

class GraphicsSceneToolPalette : public QObject
{
    public:
        explicit GraphicsSceneToolPalette(const QGraphicsScene& scene):
            m_scene{scene}
        {

        }
        virtual ~GraphicsSceneToolPalette();

        QPointF scenePoint;

        const QGraphicsScene& scene() const
        { return m_scene; }

    private:
        const QGraphicsScene& m_scene;
};
