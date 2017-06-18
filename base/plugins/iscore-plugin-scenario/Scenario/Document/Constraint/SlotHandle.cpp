#include <Process/Style/ScenarioStyle.hpp>
#include <QCursor>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPoint>
#include <Scenario/Document/Constraint/ConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ConstraintView.hpp>
#include <qnamespace.h>
#include <QGraphicsScene>
#include <QGraphicsView>
#include "SlotHandle.hpp"

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class ConstraintPresenter;
SlotHandle::SlotHandle(const ConstraintPresenter& slotView, int slotIndex, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_presenter{slotView}
    , m_width{slotView.view()->boundingRect().width()}
    , m_slotIndex{slotIndex}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setCursor(Qt::SizeVerCursor);
}

int SlotHandle::slotIndex() const
{
  return m_slotIndex;
}

void SlotHandle::setSlotIndex(int v)
{
  m_slotIndex = v;
}

QRectF SlotHandle::boundingRect() const
{
  return {1., 0., m_width - 2., handleHeight()};
}

void SlotHandle::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto& style = ScenarioStyle::instance();

  painter->fillRect(boundingRect(), style.ProcessViewBorder.getColor());
}

void SlotHandle::setWidth(qreal width)
{
  m_width = width;
  prepareGeometryChange();
}

void SlotHandle::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_presenter.pressed(event->scenePos());
}

void SlotHandle::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  static bool moving = false;
  if(!moving)
  {
    moving = true;
    m_presenter.moved(event->scenePos());

    this->scene()->views()[0]->ensureVisible(this);
    moving = false;
  }
}

void SlotHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_presenter.released(event->scenePos());
}



InactiveSlotHandle::InactiveSlotHandle(
    const ConstraintPresenter& slotView,
    int slotIndex,
    QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_width{slotView.view()->boundingRect().width()}
    , m_slotIndex{slotIndex}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setCursor(Qt::SizeVerCursor);
}

int InactiveSlotHandle::slotIndex() const
{
  return m_slotIndex;
}

void InactiveSlotHandle::setSlotIndex(int v)
{
  m_slotIndex = v;
}

QRectF InactiveSlotHandle::boundingRect() const
{
  return {1., 0., m_width - 2., handleHeight()};
}

void InactiveSlotHandle::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto& style = ScenarioStyle::instance();

  painter->fillRect(boundingRect(), style.ConstraintHeaderText.getColor());
}

void InactiveSlotHandle::setWidth(qreal width)
{
  m_width = width;
  prepareGeometryChange();
}

}
