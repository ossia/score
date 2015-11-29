#pragma once
#include <qobject.h>
#include <qpoint.h>

class QGraphicsScene;

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
