// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QGraphicsSceneEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <qnamespace.h>

#include "EventModel.hpp"
#include "EventPresenter.hpp"
#include "EventView.hpp"
#include <QCursor>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <score/model/ModelMetadata.hpp>

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
EventView::EventView(EventPresenter& presenter, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_presenter{presenter}
    , m_conditionItem{ScenarioStyle::instance().ConditionDefault, this}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  setAcceptDrops(true);

  m_color = presenter.model().metadata().getColor();

  m_conditionItem.setVisible(false);
  m_conditionItem.setPos(-13.5, -13.5);

  connect(&m_conditionItem, &ConditionView::pressed,
          &m_presenter, &EventPresenter::pressed);

  this->setParentItem(parent);
  this->setCursor(Qt::SizeHorCursor);

  this->setZValue(ZPos::Event);
  this->setAcceptHoverEvents(true);
}

void EventView::setCondition(const QString& cond)
{
  if (m_condition == cond)
    return;
  m_condition = cond;
  m_conditionItem.setVisible(!State::isEmptyExpression(cond));
  m_conditionItem.setToolTip(m_condition);
}

bool EventView::hasCondition() const
{
  return !m_condition.isEmpty();
}

void EventView::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = ScenarioStyle::instance();
  painter->setRenderHint(QPainter::Antialiasing, false);

  if (m_status.get() == ExecutionStatus::Editing)
    skin.EventPen.setBrush(m_color.getBrush());
  else
    skin.EventPen.setBrush(m_status.eventStatusColor().getBrush());

  if (isSelected())
  {
    skin.EventPen.setBrush(skin.EventSelected.getBrush());
  }
  skin.EventBrush = skin.EventPen.brush();

  painter->setPen(skin.EventPen);
  painter->fillRect(
      QRectF(
          QPointF(-1.3, 0), QPointF(1.3, m_extent.bottom() - m_extent.top())),
      skin.EventBrush);

#if defined(SCORE_SCENARIO_DEBUG_RECTS)
  painter->setPen(Qt::cyan);
  painter->drawRect(boundingRect());
#endif
}

void EventView::setExtent(const VerticalExtent& extent)
{
  prepareGeometryChange();
  m_extent = extent;
  m_conditionItem.changeHeight(extent.bottom() - extent.top());
  this->update();
}

void EventView::setExtent(VerticalExtent&& extent)
{
  prepareGeometryChange();
  m_conditionItem.changeHeight(extent.bottom() - extent.top());
  m_extent = std::move(extent);
  this->update();
}

void EventView::setStatus(ExecutionStatus s)
{
  m_status.set(s);
  if (s != ExecutionStatus::Editing)
    m_conditionItem.setColor(m_status.eventStatusColor());
  else
    m_conditionItem.setColor(ScenarioStyle::instance().ConditionDefault);

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

void EventView::changeColor(score::ColorRef newColor)
{
  m_color = newColor;
  this->update();
}

void EventView::setWidthScale(double d)
{
  QTransform t;
  this->setTransform(t.scale(d, 1.));
}

void EventView::changeToolTip(const QString& c)
{
  this->setToolTip(c);
}

void EventView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
    m_presenter.pressed(event->scenePos());
}

void EventView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_presenter.moved(event->scenePos());
}

void EventView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_presenter.released(event->scenePos());
}

void EventView::hoverEnterEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverEnterEvent(h);
  eventHoverEnter();
}

void EventView::hoverLeaveEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverLeaveEvent(h);
  eventHoverLeave();
}

void EventView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(event->scenePos(), event->mimeData());
}
}
