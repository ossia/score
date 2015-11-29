#include <qcolor.h>
#include <qevent.h>
#include <qflags.h>
#include <qgraphicsitem.h>
#include <qgraphicssceneevent.h>
#include <qnamespace.h>
#include <qpainter.h>
#include <qpen.h>

#include "Process/LayerView.hpp"
#include "TemporalScenarioView.hpp"

TemporalScenarioView::TemporalScenarioView(QGraphicsItem* parent) :
    LayerView {parent}
{
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
    this->setCursor(Qt::ArrowCursor);
    setAcceptDrops(true);

    this->setZValue(1);
}

TemporalScenarioView::~TemporalScenarioView()
{
}

void TemporalScenarioView::paint_impl(QPainter* painter) const
{
    if(m_lock)
    {
        painter->setBrush({Qt::red, Qt::DiagCrossPattern});
        painter->drawRect(boundingRect());
    }

    if(m_selectArea != QRectF{})
    {
        painter->setCompositionMode(QPainter::CompositionMode_Xor);
        painter->setPen(QPen{QColor{0, 0, 0, 127}, 2, Qt::DashLine, Qt::SquareCap, Qt::BevelJoin});

        painter->drawRect(m_selectArea);
    }
}


void TemporalScenarioView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->button() == Qt::LeftButton)
        emit pressed(event->scenePos());
}

void TemporalScenarioView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit moved(event->scenePos());
}

void TemporalScenarioView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit released(event->scenePos());
}

void TemporalScenarioView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    emit pressed(event->scenePos());
    emit released(event->scenePos());
    emit askContextMenu(event->screenPos(), event->scenePos());
}

void TemporalScenarioView::keyPressEvent(QKeyEvent* event)
{
    QGraphicsObject::keyPressEvent(event);
    if(event->key() == Qt::Key_Escape)
    {
        emit escPressed();
    }

    emit keyPressed(event->key());

    if(event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control)
    {
        emit keyPressed(event->key());
    }
}

void TemporalScenarioView::keyReleaseEvent(QKeyEvent *event)
{
    QGraphicsObject::keyReleaseEvent(event);
    if(event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control)
    {
        emit keyReleased(event->key());
    }
}

void TemporalScenarioView::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    emit dropReceived(event->scenePos(), event->mimeData());
}
