#include "View.hpp"

#include <Process/Style/ScenarioStyle.hpp>

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
}

View::~View() { }

void View::paint_impl(QPainter* painter) const
{
  if(m_rubberBanding && !m_rubberBandRect.isEmpty())
  {
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setCompositionMode(QPainter::CompositionMode_Xor);
    painter->setPen(
        QPen{QColor{0, 0, 0, 127}, 2, Qt::DashLine, Qt::SquareCap, Qt::BevelJoin});
    painter->setBrush(Qt::transparent);
    painter->drawRect(m_rubberBandRect);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
  }
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if(event->button() == Qt::LeftButton)
  {
    m_rubberBanding = true;
    m_rubberBandOrigin = event->pos();
    m_rubberBandRect = QRectF{m_rubberBandOrigin, m_rubberBandOrigin};
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
    update();
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
    const bool cumulation = event->modifiers() & Qt::ControlModifier;
    areaSelectRequested(m_rubberBandRect, cumulation);
    m_rubberBandRect = {};
    update();
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
