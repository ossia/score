#include <QEvent>
#include <QFlags>
#include <QGraphicsSceneEvent>
#include <QKeyEvent>
#include <QPainter>
#include <qnamespace.h>

#include "CurveView.hpp"


class QWidget;

namespace Curve
{
View::View(QQuickPaintedItem* parent) : QQuickPaintedItem{parent}
{
  this->setFlags(ItemClipsChildrenToShape/* | ItemIsFocusable*/);
  this->setZ(1);
}

View::~View()
{
}

void View::paint(
    QPainter* painter)
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

void View::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
    emit pressed(mapToScene(event->localPos()));
  event->accept();
}

void View::mouseDoubleClickEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
    emit doubleClick(mapToScene(event->localPos()));
  event->accept();
}

void View::mouseMoveEvent(QMouseEvent* event)
{
  emit moved(mapToScene(event->localPos()));
  event->accept();
}

void View::mouseReleaseEvent(QMouseEvent* event)
{
  emit released(mapToScene(event->localPos()));
  event->accept();
}

void View::keyPressEvent(QKeyEvent* ev)
{
  emit keyPressed(ev->key());
  ev->accept();
}

void View::keyReleaseEvent(QKeyEvent* ev)
{
  emit keyReleased(ev->key());
  ev->accept();
}

/*void View::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  emit contextMenuRequested(ev->screenPos(), ev->scenePos());
}
*/
void View::setRect(const QRectF& theRect)
{
 // prepareGeometryChange();
  m_rect = theRect;
  update();
}
}
