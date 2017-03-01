#include <Process/Style/ScenarioStyle.hpp>
#include <QPainter>

#include "LayerView.hpp"


class QWidget;
namespace Process
{
LayerView::~LayerView() = default;

LayerView::LayerView(QQuickPaintedItem* parent) : QQuickPaintedItem{parent}
{
  setAcceptedMouseButtons(Qt::AllButtons);
    this->setFlag(QQuickPaintedItem::ItemClipsChildrenToShape, true);
//  this->setCacheMode(QQuickPaintedItem::NoCache);
}

void LayerView::paint(
    QPainter* painter)
{
  paint_impl(painter);
}

void LayerView::setHeight(qreal height)
{
  QQuickPaintedItem::setHeight(height);
  emit heightChanged();
}

void LayerView::setWidth(qreal width)
{
  QQuickPaintedItem::setWidth(width);
  emit widthChanged();
}

}
