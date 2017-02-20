#include "TriggerView.hpp"

class QGraphicsSceneMouseEvent;
#include <QPainter>


class QWidget;

namespace Scenario
{
TriggerView::TriggerView(QQuickPaintedItem* parent)
    : QGraphicsSvgItem{":/images/trigger.svg", parent}
{
  this->setCacheMode(QQuickPaintedItem::NoCache);
  this->setScale(1.5);
  setFlag(ItemStacksBehindParent, true);
}

void TriggerView::mousePressEvent(QMouseEvent* ev)
{
  emit pressed();
}
}
