#pragma once

#include <QGraphicsItem>
#include <wobjectdefs.h>
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
class SCORE_PLUGIN_CURVE_EXPORT View final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(View)
  Q_INTERFACES(QGraphicsItem)
public:
  explicit View(QGraphicsItem* parent);
  ~View() override;

  void setRect(const QRectF& theRect);
  QRectF boundingRect() const override;

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void setSelectionArea(const QRectF&);
  QPixmap pixmap();

public:
  void pressed(QPointF arg_1) W_SIGNAL(pressed, arg_1);
  void moved(QPointF arg_1) W_SIGNAL(moved, arg_1);
  void released(QPointF arg_1) W_SIGNAL(released, arg_1);
  void doubleClick(QPointF arg_1) W_SIGNAL(doubleClick, arg_1);

  void escPressed() W_SIGNAL(escPressed);

  void keyPressed(int arg_1) W_SIGNAL(keyPressed, arg_1);
  void keyReleased(int arg_1) W_SIGNAL(keyReleased, arg_1);

  void contextMenuRequested(const QPoint& arg_1, const QPointF& arg_2) W_SIGNAL(contextMenuRequested, arg_1, arg_2);

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
