#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QGraphicsSceneEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <qnamespace.h>

#include "ConditionView.hpp"
#include "EventModel.hpp"
#include "EventPresenter.hpp"
#include "EventView.hpp"
#include <QCursor>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <iscore/model/ModelMetadata.hpp>


class QWidget;

namespace Scenario
{
EventView::EventView(EventPresenter& presenter, QQuickPaintedItem* parent)
    : GraphicsItem{parent}, m_presenter{presenter}
{
  //this->setCacheMode(QQuickPaintedItem::NoCache);
  //setAcceptDrops(true);

  m_color = presenter.model().metadata().getColor();

  m_conditionItem
      = new ConditionView(ScenarioStyle::instance().ConditionDefault, this);
  m_conditionItem->setVisible(false);
  m_conditionItem->setPosition(QPointF(-13.5, -13.5));

  this->setParentItem(parent);
  this->setCursor(Qt::SizeHorCursor);

  this->setZ(ZPos::Event);
  this->setWidth(6);
  this->setAcceptHoverEvents(true);
}

void EventView::setCondition(const QString& cond)
{
  if (m_condition == cond)
    return;
  m_condition = cond;
  m_conditionItem->setVisible(!State::isTrueExpression(cond));
  //m_conditionItem->setToolTip(m_condition);
}

bool EventView::hasCondition() const
{
  return !m_condition.isEmpty();
}

void EventView::setTrigger(const QString& trig)
{
  if (m_trigger == trig)
    return;
  m_trigger = trig;
}

bool EventView::hasTrigger() const
{
  return !m_trigger.isEmpty();
}

void EventView::paint(
    QPainter* painter)
{
  auto& skin = ScenarioStyle::instance();
  painter->setRenderHint(QPainter::Antialiasing, false);

  if (m_status.get() == ExecutionStatus::Editing)
    skin.EventPen.setBrush(m_color.getColor());
  else
    skin.EventPen.setBrush(m_status.eventStatusColor().getColor());

  if (isSelected())
  {
    skin.EventPen.setBrush(skin.EventSelected.getColor());
  }
  skin.EventBrush = skin.EventPen.brush();

  painter->setPen(skin.EventPen);
  painter->fillRect(
      QRectF(
          QPointF(-1.3, 0), QPointF(1.3, m_extent.bottom() - m_extent.top())),
      skin.EventBrush);

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
  painter->setPen(Qt::cyan);
  painter->drawRect(boundingRect());
#endif
}

void EventView::setExtent(const VerticalExtent& extent)
{
  //prepareGeometryChange();
  m_extent = extent;
  setHeight(qreal(m_extent.bottom() - m_extent.top() + 20));
  m_conditionItem->changeHeight(extent.bottom() - extent.top());
  this->update();
}

void EventView::setExtent(VerticalExtent&& extent)
{
  //prepareGeometryChange();
  m_conditionItem->changeHeight(extent.bottom() - extent.top());
  m_extent = std::move(extent);
  setHeight(qreal(m_extent.bottom() - m_extent.top() + 20));
  this->update();
}

void EventView::setStatus(ExecutionStatus s)
{
  m_status.set(s);
  if (s != ExecutionStatus::Editing)
    m_conditionItem->setColor(m_status.eventStatusColor());
  else
    m_conditionItem->setColor(ScenarioStyle::instance().ConditionDefault);

  update();
}

void EventView::setSelected(bool selected)
{
  m_selected = selected;
  update();
}

bool EventView::isSelected() const
{
  return m_selected;
}

void EventView::changeColor(iscore::ColorRef newColor)
{
  m_color = newColor;
  this->update();
}

void EventView::setWidthScale(double d)
{
  //QTransform t;
  //this->setTransform(t.scale(d, 1.));
}

void EventView::changeToolTip(const QString& c)
{
  //this->setToolTip(c);
}

void EventView::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
    emit m_presenter.pressed(mapToScene(event->localPos()));
}

void EventView::mouseMoveEvent(QMouseEvent* event)
{
  emit m_presenter.moved(mapToScene(event->localPos()));
}

void EventView::mouseReleaseEvent(QMouseEvent* event)
{
  emit m_presenter.released(mapToScene(event->localPos()));
}

void EventView::hoverEnterEvent(QHoverEvent* h)
{
  QQuickPaintedItem::hoverEnterEvent(h);
  emit eventHoverEnter();
}

void EventView::hoverLeaveEvent(QHoverEvent* h)
{
  QQuickPaintedItem::hoverLeaveEvent(h);
  emit eventHoverLeave();
}

void EventView::dropEvent(QDropEvent* event)
{
  emit dropReceived(mapToScene(event->pos()), event->mimeData());
}
}
