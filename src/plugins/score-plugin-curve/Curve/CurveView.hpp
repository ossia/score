#pragma once

#include <QGraphicsItem>
#include <QPoint>
#include <QRect>

#include <score_plugin_curve_export.h>

#include <verdigris>

class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneMouseEvent;
class QKeyEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Curve
{
class SCORE_PLUGIN_CURVE_EXPORT View final : public QObject, public QGraphicsItem
{
  W_OBJECT(View)
  Q_INTERFACES(QGraphicsItem)
public:
  explicit View(QGraphicsItem* parent) noexcept;
  ~View() override;

  void setDefaultWidth(double w) noexcept;
  void setRect(const QRectF& theRect) noexcept;
  QRectF boundingRect() const override;

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void setSelectionArea(const QRectF&) noexcept;
  QPixmap pixmap() noexcept;

  void setValueTooltip(QPointF pos, const QString&) noexcept;

public:
  void pressed(QPointF arg_1) E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, pressed, arg_1)
  void moved(QPointF arg_1) E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, moved, arg_1)
  void released(QPointF arg_1) E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, released, arg_1)
  void doubleClick(QPointF arg_1) E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, doubleClick, arg_1)

  void escPressed() E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, escPressed)

  void keyPressed(int arg_1) E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, keyPressed, arg_1)
  void keyReleased(int arg_1) E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, keyReleased, arg_1)

  void contextMenuRequested(const QPoint& arg_1, const QPointF& arg_2)
      E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, contextMenuRequested, arg_1, arg_2)

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
  QPointF m_tooltipPos;
  QString m_tooltip;
  double m_defaultW{};
};
}
