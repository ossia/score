// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QEvent>
#include <QFlags>
#include <QGraphicsSceneEvent>
#include <QKeyEvent>
#include <QPainter>
#include <qnamespace.h>

#include "CurveView.hpp"

class QStyleOptionGraphicsItem;
class QWidget;

namespace Curve
{
View::View(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  this->setFlags(ItemIsFocusable);
  this->setZValue(1);
}

View::~View()
{
}

void View::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if (m_selectArea != QRectF{})
  {
    painter->setPen(Qt::white);
    painter->drawRect(m_selectArea);
  }
}

void View::setSelectionArea(const QRectF& rect)
{
  m_selectArea = rect;
  update();
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
    pressed(event->scenePos());
  event->accept();
}

void View::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
    doubleClick(event->scenePos());
  event->accept();
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  moved(event->scenePos());
  event->accept();
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  released(event->scenePos());
  event->accept();
}

void View::keyPressEvent(QKeyEvent* ev)
{
  keyPressed(ev->key());
  ev->accept();
}

void View::keyReleaseEvent(QKeyEvent* ev)
{
  keyReleased(ev->key());
  ev->accept();
}

void View::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  contextMenuRequested(ev->screenPos(), ev->scenePos());
}

void View::setRect(const QRectF& theRect)
{
  prepareGeometryChange();
  m_rect = theRect;
  setVisible(m_rect.width() > 5);
  update();
}

QRectF View::boundingRect() const
{
  return m_rect;
}
}
