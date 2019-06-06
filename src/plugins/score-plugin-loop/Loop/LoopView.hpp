#pragma once
#include <Process/LayerView.hpp>

#include <QRect>

#include <verdigris>

class QGraphicsItem;
class QPainter;

namespace Loop
{
class LayerView final : public Process::LayerView
{
  W_OBJECT(LayerView)
public:
  LayerView(QGraphicsItem* parent);

  ~LayerView();

  void setSelectionArea(QRectF);

protected:
  void paint_impl(QPainter*) const override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;
};
}
