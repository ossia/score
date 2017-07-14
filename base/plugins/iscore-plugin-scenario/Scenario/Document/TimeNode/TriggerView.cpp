// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TriggerView.hpp"
#include <QPainter>

namespace Scenario
{
TriggerView::TriggerView(QGraphicsItem* parent)
    : QGraphicsSvgItem{":/images/trigger.svg", parent}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setScale(1.5);
  setFlag(ItemStacksBehindParent, true);
}

void TriggerView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  emit pressed();
}
}
