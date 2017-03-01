#include <QColor>
#include <QCursor>
#include <QEvent>
#include <QFlags>
#include <QQuickPaintedItem>
#include <QGraphicsSceneEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPen>
#include <qnamespace.h>

#include "TemporalScenarioView.hpp"
#include <Process/LayerView.hpp>
namespace Scenario
{
TemporalScenarioView::TemporalScenarioView(QQuickPaintedItem* parent)
    : LayerView{parent}
{
  /*
  this->setFlags(
      ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);

  setAcceptDrops(true);
*/
  this->setZ(1);
}

TemporalScenarioView::~TemporalScenarioView() = default;

void TemporalScenarioView::paint_impl(QPainter* painter) const
{
  painter->setRenderHint(QPainter::Antialiasing, false);
  if (m_lock)
  {
    painter->setBrush({Qt::red, Qt::DiagCrossPattern});
    painter->drawRect(boundingRect());
  }

  if (m_selectArea != QRectF{})
  {
    painter->setCompositionMode(QPainter::CompositionMode_Xor);
    painter->setPen(QPen{QColor{0, 0, 0, 127}, 2, Qt::DashLine, Qt::SquareCap,
                         Qt::BevelJoin});
    painter->drawRect(m_selectArea);
    painter->setCompositionMode(
        QPainter::CompositionMode::CompositionMode_SourceOver);
  }

  if(m_dragLine)
  {
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF& rec = *m_dragLine;
    painter->setPen(QPen{Qt::gray, 2, Qt::DashLine});
    painter->drawLine(rec.topLeft(), rec.bottomLeft());
    painter->drawLine(rec.bottomLeft(), rec.bottomRight());
    painter->drawEllipse(rec.bottomRight(), 3, 3);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }
}

void TemporalScenarioView::drawDragLine(QPointF left, QPointF right)
{
  m_dragLine = QRectF(left, right);

  update();
}


void TemporalScenarioView::stopDrawDragLine()
{
  m_dragLine = iscore::none;
}

void TemporalScenarioView::movedAsked(const QPointF& p)
{
  QRectF r = QRectF{m_previousPoint.x(), m_previousPoint.y(), 1, 1};
  //ensureVisible(mapRectFromScene(r), 30, 30);
  emit moved(p);
  m_previousPoint
      = p; // we use the last pos, because if not there's a larsen and crash
}

void TemporalScenarioView::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
    emit pressed(mapToScene(event->localPos()));

  event->accept();
}

void TemporalScenarioView::mouseMoveEvent(QMouseEvent* event)
{
  emit moved(mapToScene(event->localPos()));

  event->accept();
}

void TemporalScenarioView::mouseReleaseEvent(QMouseEvent* event)
{
  emit released(mapToScene(event->localPos()));

  event->accept();
}

void TemporalScenarioView::mouseDoubleClickEvent(
    QMouseEvent* event)
{
  emit doubleClick(event->pos());

  event->accept();
}
/*
void TemporalScenarioView::contextMenuEvent(
    QGraphicsSceneContextMenuEvent* event)
{
  emit askContextMenu(event->screenPos(), mapToScene(event->localPos()));

  event->accept();
}
*/

void TemporalScenarioView::keyPressEvent(QKeyEvent* event)
{
  QQuickPaintedItem::keyPressEvent(event);
  if (event->key() == Qt::Key_Escape)
  {
    emit escPressed();
  }
  else if (event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control)
  {
    emit keyPressed(event->key());
  }

  event->accept();
}

void TemporalScenarioView::keyReleaseEvent(QKeyEvent* event)
{
  QQuickPaintedItem::keyReleaseEvent(event);
  if (event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control)
  {
    emit keyReleased(event->key());
  } /*
   else if( !event->isAutoRepeat())
   {
       if(event->key() == Qt::Key_C)
       {
           emit keyReleased(event->key());
       }
   }
   */

  event->accept();
}

void TemporalScenarioView::dragEnterEvent(QDragEnterEvent* event)
{
  emit dragEnter(event->pos(), event->mimeData());

  event->accept();
}

void TemporalScenarioView::dragMoveEvent(QDragMoveEvent* event)
{
  emit dragMove(event->pos(), event->mimeData());

  event->accept();
}

void TemporalScenarioView::dragLeaveEvent(QDragLeaveEvent* event)
{
  emit dragLeave(QPointF{}, nullptr);

  event->accept();
}

void TemporalScenarioView::dropEvent(QDropEvent* event)
{
  emit dropReceived(event->pos(), event->mimeData());

  event->accept();
}
}
