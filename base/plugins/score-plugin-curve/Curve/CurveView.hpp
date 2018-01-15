#pragma once

#include <QGraphicsItem>
#include <QPoint>
#include <QRect>
#include <score_plugin_curve_export.h>

class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneMouseEvent;
class QKeyEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Curve
{
class SCORE_PLUGIN_CURVE_EXPORT View final : public QObject,
                                              public QGraphicsItem
{
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)
public:
  explicit View(QGraphicsItem* parent);
  virtual ~View();

  void setRect(const QRectF& theRect);
  QRectF boundingRect() const override;

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void setSelectionArea(const QRectF&);

Q_SIGNALS:
  void pressed(QPointF);
  void moved(QPointF);
  void released(QPointF);
  void doubleClick(QPointF);

  void escPressed();

  void keyPressed(int);
  void keyReleased(int);

  void contextMenuRequested(const QPoint&, const QPointF&);

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  void keyPressEvent(QKeyEvent* ev) override;
  void keyReleaseEvent(QKeyEvent* ev) override;

  void contextMenuEvent(QGraphicsSceneContextMenuEvent*) override;

  QRectF m_rect; // The rect in which the whole curve must fit.
  QRectF m_selectArea;
};
}
