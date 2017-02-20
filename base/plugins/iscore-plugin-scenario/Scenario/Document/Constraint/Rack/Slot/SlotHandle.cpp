#include <Process/Style/ScenarioStyle.hpp>
#include <QCursor>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPoint>
#include <qnamespace.h>

#include "SlotHandle.hpp"
#include <Scenario/Document/Constraint/Rack/Slot/SlotPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotView.hpp>


class QWidget;

namespace Scenario
{
SlotHandle::SlotHandle(const SlotView& slotView, QQuickPaintedItem* parent)
    : QQuickPaintedItem{parent}
    , m_slotView{slotView}
    , m_width{slotView.boundingRect().width()}
{
  this->setCacheMode(QQuickPaintedItem::NoCache);
  this->setCursor(Qt::SizeVerCursor);
  m_pen.setWidth(0);
}

QRectF SlotHandle::boundingRect() const
{
  return {0, -handleHeight() / 2., m_width, handleHeight()};
}

void SlotHandle::paint(
    QPainter* painter)
{
  m_pen.setColor(ScenarioStyle::instance().ProcessViewBorder.getColor());
  painter->setPen(m_pen);
  painter->setBrush(m_pen.color());

  painter->drawLine(0, -handleHeight() / 2., m_width, -handleHeight() / 2.);
}

void SlotHandle::setWidth(qreal width)
{
  m_width = width;
  prepareGeometryChange();
}

void SlotHandle::mousePressEvent(QMouseEvent* event)
{
  m_slotView.presenter.pressed(event->scenePos());
}

void SlotHandle::mouseMoveEvent(QMouseEvent* event)
{
  m_slotView.presenter.moved(event->scenePos());
}

void SlotHandle::mouseReleaseEvent(QMouseEvent* event)
{
  m_slotView.presenter.released(event->scenePos());
}
}
