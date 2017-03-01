#pragma once

#include <QQuickPaintedItem>
#include <QPoint>
#include <QRect>
#include <iscore_plugin_curve_export.h>

class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneMouseEvent;
class QKeyEvent;
class QPainter;

class QWidget;

namespace Curve
{
class ISCORE_PLUGIN_CURVE_EXPORT View final :public QQuickPaintedItem
{
  Q_OBJECT
public:
  explicit View(QQuickPaintedItem* parent);
  virtual ~View();

  void setRect(const QRectF& theRect);

  void paint(
      QPainter* painter) override;

  void setSelectionArea(const QRectF&);

signals:
  void pressed(QPointF);
  void moved(QPointF);
  void released(QPointF);
  void doubleClick(QPointF);

  void escPressed();

  void keyPressed(int);
  void keyReleased(int);

  void contextMenuRequested(const QPoint&, const QPointF&);

protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;

  void mouseMoveEvent(QMouseEvent* event) override;

  void mouseReleaseEvent(QMouseEvent* event) override;

  void keyPressEvent(QKeyEvent* ev) override;
  void keyReleaseEvent(QKeyEvent* ev) override;

//  void contextMenuEvent(QGraphicsSceneContextMenuEvent*) override;

private:
  QRectF m_selectArea;
};
}
