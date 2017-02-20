#pragma once
#include <QObject>
#include <QPoint>
#include <iscore_lib_base_export.h>

class QQuickItem;

class ISCORE_LIB_BASE_EXPORT GraphicsSceneToolPalette : public QObject
{
public:
  explicit GraphicsSceneToolPalette(const QQuickItem& scene)
      : m_scene{scene}
  {
  }
  virtual ~GraphicsSceneToolPalette();

  QPointF scenePoint;

  const QQuickItem& scene() const
  {
    return m_scene;
  }

private:
  const QQuickItem& m_scene;
};
