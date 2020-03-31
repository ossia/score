// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "EventView.hpp"

#include "EventModel.hpp"
#include "EventPresenter.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/VerticalExtent.hpp>

#include <score/model/ModelMetadata.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QCursor>
#include <qnamespace.h>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Scenario::EventView)

namespace Scenario
{
EventView::EventView(EventPresenter& presenter, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_presenter{presenter}
    , m_conditionItem{presenter.model(), this}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  setAcceptDrops(true);

  m_conditionItem.setVisible(false);
  m_conditionItem.setPos(-13.5, -13.5);

  connect(
      &m_conditionItem,
      &ConditionView::pressed,
      &m_presenter,
      &EventPresenter::pressed);

  this->setParentItem(parent);
  this->setCursor(Qt::SizeHorCursor);

  this->setZValue(ZPos::Event);
  this->setAcceptHoverEvents(true);
}

EventView::~EventView() {}

void EventView::setStatus(ExecutionStatus status)
{
  if(status == ExecutionStatus::Happened)
  {
    m_execPing.start();
  }
  update();
  conditionItem().update();
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
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto& skin = Process::Style::instance();
  painter->setRenderHint(QPainter::Antialiasing, false);

  const auto rect = QRectF(QPointF(-1.3, 0.), QPointF(1.3, m_height));
  if (Q_UNLIKELY(isSelected()))
  {
    painter->fillRect(rect, skin.EventSelected());
  }
  else
  {
    if(Q_UNLIKELY(m_execPing.running()))
    {
      const auto& nextPen = m_execPing.getNextPen(
            m_presenter.model().color(skin).color(),
            skin.EventHappened().color(),
            skin.StateDot().main.pen_cosmetic);
      painter->fillRect(rect, nextPen.brush());
      update();
    }
    else
    {
      painter->fillRect(rect, m_presenter.model().color(skin));
    }
  }

#if defined(SCORE_SCENARIO_DEBUG_RECTS)
  painter->setPen(Qt::cyan);
  painter->drawRect(boundingRect());
#endif
}

void EventView::setExtent(const VerticalExtent& extent)
{
  prepareGeometryChange();
  const auto h = extent.bottom() - extent.top();
  m_conditionItem.changeHeight(h);
  m_height = h;
  setFlag(ItemHasNoContents, h < 3.);
  this->update();
}

void EventView::setExtent(VerticalExtent&& extent)
{
  prepareGeometryChange();
  const auto h = extent.bottom() - extent.top();
  m_conditionItem.changeHeight(h);
  m_height = h;
  setFlag(ItemHasNoContents, h < 3.);
  this->update();
}

void EventView::setSelected(bool selected)
{
  m_selected = selected;
  m_conditionItem.setSelected(selected);
  setZValue(m_selected ? ZPos::SelectedEvent: ZPos::Event);
  m_conditionItem.setZValue(m_selected ? ZPos::SelectedEvent: ZPos::Event);
  update();
}

bool EventView::isSelected() const
{
  return m_selected;
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
  dropReceived(event->scenePos(), *event->mimeData());
}
}
