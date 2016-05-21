#include <QColor>
#include <QEvent>
#include <QFlags>
#include <QGraphicsItem>
#include <QGraphicsSceneEvent>
#include <qnamespace.h>
#include <QPainter>
#include <QPen>
#include <QCursor>
#include <QKeyEvent>

#include <Process/LayerView.hpp>
#include "TemporalScenarioView.hpp"
namespace Scenario
{
TemporalScenarioView::TemporalScenarioView(QGraphicsItem* parent) :
    LayerView {parent}
{
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
    this->setCursor(Qt::ArrowCursor);
    setAcceptDrops(true);

    this->setZValue(1);
}

TemporalScenarioView::~TemporalScenarioView() = default;

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

void TemporalScenarioView::movedAsked(const QPointF& p)
{
    QRectF r = QRectF{m_previousPoint.x(), m_previousPoint.y() , 1, 1};
    ensureVisible(mapRectFromScene(r), 30, 30);
    emit moved(p);
    m_previousPoint = p; // we use the last pos, because if not there's a larsen and crash
}


void TemporalScenarioView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->button() == Qt::LeftButton)
        emit pressed(event->scenePos());

    event->accept();
}

void TemporalScenarioView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit moved(event->scenePos());

    event->accept();
}

void TemporalScenarioView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit released(event->scenePos());

    event->accept();
}

void TemporalScenarioView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    emit pressed(event->scenePos());
    emit released(event->scenePos());
    emit askContextMenu(event->screenPos(), event->scenePos());

    event->accept();
}

void TemporalScenarioView::keyPressEvent(QKeyEvent* event)
{
    QGraphicsObject::keyPressEvent(event);
    if(event->key() == Qt::Key_Escape)
    {
        emit escPressed();
    }
    else if(event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control)
    {
        emit keyPressed(event->key());
    }

    event->accept();
}

void TemporalScenarioView::keyReleaseEvent(QKeyEvent *event)
{
    QGraphicsObject::keyReleaseEvent(event);
    if(event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control)
    {
        emit keyReleased(event->key());
    }

    event->accept();
}

void TemporalScenarioView::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    emit dropReceived(event->scenePos(), event->mimeData());

    event->accept();
}
}
