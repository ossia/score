#include "View.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Nodal::View)

namespace Nodal
{

View::View(QGraphicsItem* parent)
    : LayerView{parent}
{
  setFlag(ItemClipsChildrenToShape);

  m_selectionRect = new QGraphicsRectItem(this);
  m_selectionRect->setZValue(1000.);
  m_selectionRect->setPen(QPen{QColor{0, 255, 255, 200}, 1, Qt::DashLine, Qt::SquareCap, Qt::BevelJoin});
  m_selectionRect->setBrush(QColor{0, 255, 255, 20});
  m_selectionRect->setAcceptedMouseButtons(Qt::NoButton);
  m_selectionRect->setVisible(false);
}

View::~View() { }

void View::paint_impl(QPainter* painter) const { }

void View::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if(event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier))
  {
    m_rubberBanding = true;
    m_rubberBandOrigin = event->pos();
    m_rubberBandRect = QRectF{m_rubberBandOrigin, m_rubberBandOrigin};
    m_selectionRect->setVisible(true);
    m_selectionRect->setRect(QRectF{});
    event->accept();
  }
  else
  {
    LayerView::mousePressEvent(event);
  }
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_rubberBanding && (event->buttons() & Qt::LeftButton))
  {
    m_rubberBandRect = QRectF{m_rubberBandOrigin, event->pos()}.normalized();
    m_selectionRect->setRect(m_rubberBandRect);
    event->accept();
  }
  else
  {
    LayerView::mouseMoveEvent(event);
  }
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(event->button() == Qt::LeftButton && m_rubberBanding)
  {
    m_rubberBanding = false;
    m_selectionRect->setVisible(false);
    m_selectionRect->setRect(QRectF{});
    const bool cumulation = event->modifiers() & Qt::ControlModifier;
    areaSelectRequested(m_rubberBandRect, cumulation);
    m_rubberBandRect = {};
    event->accept();
  }
  else
  {
    LayerView::mouseReleaseEvent(event);
  }
}

void View::dragEnterEvent(QGraphicsSceneDragDropEvent* event) { }

void View::dragLeaveEvent(QGraphicsSceneDragDropEvent* event) { }

void View::dragMoveEvent(QGraphicsSceneDragDropEvent* event) { }

void View::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(event->pos(), *event->mimeData());
}

}
