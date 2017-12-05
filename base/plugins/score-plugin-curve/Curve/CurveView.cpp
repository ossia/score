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
    emit pressed(event->scenePos());
  event->accept();
}

void View::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
    emit doubleClick(event->scenePos());
  event->accept();
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  emit moved(event->scenePos());
  event->accept();
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  emit released(event->scenePos());
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

void View::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  emit contextMenuRequested(ev->screenPos(), ev->scenePos());
}

void View::setRect(const QRectF& theRect)
{
  prepareGeometryChange();
  m_rect = theRect;
  update();
}

QRectF View::boundingRect() const
{
  return m_rect;
}

QPixmap View::pixmap()
{
    // Retrieve the bounding rect
    QRect rect = boundingRect().toRect();
    if (rect.isNull() || !rect.isValid()) {
        return QPixmap();
    }

    // Create the pixmap
    QPixmap pixmap(rect.size());
    pixmap.fill(Qt::transparent);

    // Render
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.translate(-rect.topLeft());
    paint(&painter, nullptr, nullptr);
    for (QGraphicsItem* child : childItems()) {
        painter.save();
        painter.translate(child->mapToParent(pos()));
        child->paint(&painter, nullptr, nullptr);
        painter.restore();
    }

    painter.end();

    return pixmap;
}
}
