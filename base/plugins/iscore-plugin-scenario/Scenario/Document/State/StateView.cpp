#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QCursor>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPen>
#include <qnamespace.h>

#include "StateMenuOverlay.hpp"
#include "StatePresenter.hpp"
#include "StateView.hpp"

class QStyleOptionGraphicsItem;
class QWidget;
namespace Scenario
{
StateView::StateView(StatePresenter& pres, QGraphicsItem* parent)
    : QGraphicsItem(parent), m_presenter{pres}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setParentItem(parent);

  this->setCursor(QCursor(Qt::SizeAllCursor));
  this->setZValue(ZPos::State);
  this->setAcceptDrops(true);
  this->setAcceptHoverEvents(true);
}

void StateView::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, true);
  auto& skin = ScenarioStyle::instance();
  painter->setPen(skin.NoPen);
  skin.StateTemporalPointBrush =
      m_selected ? skin.StateSelected.getColor() : skin.StateDot.getColor();

  auto status = m_status.get();
  if (status != ExecutionStatus::Editing)
    skin.StateTemporalPointBrush = m_status.stateStatusColor().getColor();

  if (m_containMessage)
  {
    painter->setBrush(skin.StateOutline.getColor());
    painter->drawEllipse(
        {0., 0.},
        m_radiusFull * m_dilatationFactor,
        m_radiusFull * m_dilatationFactor);
  }

  skin.StateTemporalPointPen.setColor(skin.StateTemporalPointBrush.color());

  painter->setBrush(skin.StateTemporalPointBrush);
  painter->setPen(skin.StateTemporalPointPen);
  const qreal r = m_radiusPoint * m_dilatationFactor;
  painter->drawEllipse(QPointF{0., 0.}, r, r);

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
  painter->setBrush(Qt::NoBrush);
  painter->setPen(Qt::darkYellow);
  painter->drawRect(boundingRect());
#endif

  painter->setRenderHint(QPainter::Antialiasing, false);
}

void StateView::setContainMessage(bool arg)
{
  m_containMessage = arg;
  update();
}

void StateView::setSelected(bool b)
{
  m_selected = b;
  setDilatation(m_selected ? 1.5 : 1);

  if(m_hasOverlay)
  {
      if(b)
      {
        m_overlay = new StateMenuOverlay{this};
        m_overlay->setPos(14, -14);
      }
      else
      {
        delete m_overlay;
        m_overlay = nullptr;
      }
  }
}

void StateView::setStatus(ExecutionStatus status)
{
  if (m_status.get() == status)
    return;
  m_status.set(status);
  update();
}

void StateView::disableOverlay()
{
    m_hasOverlay = false;
}

void StateView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
    emit m_presenter.pressed(event->scenePos());
}

void StateView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  emit m_presenter.moved(event->scenePos());
}

void StateView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  emit m_presenter.released(event->scenePos());
}

void StateView::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
}

void StateView::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
}
void StateView::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  setDilatation(1.5);
}

void StateView::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  setDilatation(m_selected ? 1.5 : 1);
}

void StateView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  emit dropReceived(event->mimeData());
}

void StateView::setDilatation(double val)
{
  prepareGeometryChange();
  m_dilatationFactor = val;
  this->update();
}
}
