#pragma once
#include <Process/LayerView.hpp>
#include <QRect>

class QQuickPaintedItem;
class QPainter;

namespace Loop
{
class LayerView final : public Process::LayerView
{
  Q_OBJECT
public:
  LayerView(QQuickPaintedItem* parent);

  ~LayerView();

  void setSelectionArea(QRectF);

signals:
  void askContextMenu(const QPoint&, const QPointF&);
  void pressed();

protected:
  void paint_impl(QPainter*) const override;
//  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void mousePressEvent(QMouseEvent*) override;
};
}
