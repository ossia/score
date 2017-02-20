#pragma once
#include <QQuickWidget>
#include <QPoint>
#include <iscore_lib_process_export.h>
class QFocusEvent;
class QGraphicsScene;
class QKeyEvent;
class QPainterPath;
class QResizeEvent;
class QSize;
class QWheelEvent;
class SceneGraduations;

// TODO namespace !!!
class ISCORE_LIB_PROCESS_EXPORT ProcessGraphicsView final
    : public QQuickWidget
{
  Q_OBJECT
public:
  ProcessGraphicsView(QQuickItem* scene, QWidget* parent);

  void setGrid(QPainterPath&& newGrid);
  void scrollHorizontal(double dx);
signals:
  void sizeChanged(const QSize&);
  void scrolled(int);
  void zoom(QPoint pixDelta, QPointF pos);

private:
  void resizeEvent(QResizeEvent* ev) override;
//  void scrollContentsBy(int dx, int dy) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;

  SceneGraduations* m_graduations{};
  bool m_zoomModifier{false};
};
