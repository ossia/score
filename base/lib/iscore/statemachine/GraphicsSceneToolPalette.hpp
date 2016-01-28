#pragma once
#include <QObject>
#include <QPoint>
#include <iscore_lib_base_export.h>

class QGraphicsScene;

class ISCORE_LIB_BASE_EXPORT GraphicsSceneToolPalette : public QObject
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
