#pragma once
#include <QGraphicsView>
#include <wobjectdefs.h>
#include <QPoint>
#include <score_lib_process_export.h>
class QFocusEvent;
class QGraphicsScene;
class QKeyEvent;
class QPainterPath;
class QResizeEvent;
class QSize;
class QWheelEvent;
class SceneGraduations;

// TODO namespace !!!
class SCORE_LIB_PROCESS_EXPORT ProcessGraphicsView final : public QGraphicsView
{
  W_OBJECT(ProcessGraphicsView)
public:
  ProcessGraphicsView(QGraphicsScene* scene, QWidget* parent);

  void scrollHorizontal(double dx);
public:
  void sizeChanged(const QSize& arg_1) W_SIGNAL(sizeChanged, arg_1);
  void scrolled(int arg_1) W_SIGNAL(scrolled, arg_1);
  void focusedOut() W_SIGNAL(focusedOut);
  void horizontalZoom(QPointF pixDelta, QPointF pos) W_SIGNAL(horizontalZoom, pixDelta, pos);
  void verticalZoom(QPointF pixDelta, QPointF pos) W_SIGNAL(verticalZoom, pixDelta, pos);

private:
  void resizeEvent(QResizeEvent* ev) override;
  void scrollContentsBy(int dx, int dy) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void drawBackground(QPainter* painter, const QRectF& rect) override;

  bool m_hZoom{false};
  bool m_vZoom{false};

  std::chrono::steady_clock::time_point m_lastwheel;
};
