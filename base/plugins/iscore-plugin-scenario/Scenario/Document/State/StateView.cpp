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


class QWidget;
namespace Scenario
{
StateView::StateView(StatePresenter& pres, QQuickPaintedItem* parent)
    : GraphicsItem(parent), m_presenter{pres}
{
  // this->setCacheMode(QQuickPaintedItem::NoCache);
  this->setParentItem(parent);

  this->setCursor(QCursor(Qt::SizeAllCursor));
  this->setZ(ZPos::State);
  // this->setAcceptDrops(true);
  this->setAcceptHoverEvents(true);
  m_color = ScenarioStyle::instance().StateOutline;
}

void StateView::paint(
    QPainter* painter)
{
  painter->setPen(Qt::NoPen);
  painter->setRenderHint(QPainter::Antialiasing, true);
  auto& skin = ScenarioStyle::instance();
  skin.StateTemporalPointBrush.setColor(
      m_selected ? skin.StateSelected.getColor() : skin.StateDot.getColor());

  auto status = m_status.get();
  if (status != ExecutionStatus::Editing)
    skin.StateTemporalPointBrush.setColor(
        m_status.stateStatusColor().getColor());

  if (m_containMessage)
  {
    skin.StateBrush.setColor(m_color.getColor());
    painter->setBrush(skin.StateBrush);
    painter->drawEllipse(
        {0., 0.},
        m_radiusFull * m_dilatationFactor,
        m_radiusFull * m_dilatationFactor);
  }

  painter->setBrush(skin.StateTemporalPointBrush);
  qreal r = m_radiusPoint * m_dilatationFactor;
  painter->drawEllipse({0., 0.}, r, r);

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

  if(b)
  {
    m_overlay = new StateMenuOverlay{this};
    m_overlay->setPosition(QPointF(10, -10));
  }
  else
  {
    delete m_overlay;
    m_overlay = nullptr;
  }
}

void StateView::changeColor(iscore::ColorRef c)
{
  m_color = c;
  update();
}

void StateView::setStatus(ExecutionStatus status)
{
  if (m_status.get() == status)
    return;
  m_status.set(status);
  update();
}

void StateView::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
    emit m_presenter.pressed(mapToScene(event->localPos()));
}

void StateView::mouseMoveEvent(QMouseEvent* event)
{
  emit m_presenter.moved(mapToScene(event->localPos()));
}

void StateView::mouseReleaseEvent(QMouseEvent* event)
{
  emit m_presenter.released(mapToScene(event->localPos()));
}

void StateView::hoverEnterEvent(QHoverEvent* event)
{
  //   m_dilatationFactor = 1.5;
  //   update();
}

void StateView::hoverLeaveEvent(QHoverEvent* event)
{
  //    m_dilatationFactor = m_selected ? 1.5 : 1;
  //    update();
}
void StateView::dragEnterEvent(QDragEnterEvent* event)
{
  setDilatation(1.5);
}

void StateView::dragLeaveEvent(QDragLeaveEvent* event)
{
  setDilatation(m_selected ? 1.5 : 1);
}

void StateView::dropEvent(QDropEvent* event)
{
  emit dropReceived(event->mimeData());
}

void StateView::setDilatation(double val)
{
  //prepareGeometryChange();
  m_dilatationFactor = val;
  //    this->setScale(m_dilatationFactor);
  //    emit m_presenter.askUpdate();
  this->update();
}
}
