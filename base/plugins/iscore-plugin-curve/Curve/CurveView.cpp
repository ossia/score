#include "CurveView.hpp"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

CurveView::CurveView(QGraphicsItem *parent):
    QGraphicsObject{parent}
{
    this->setFlags(ItemClipsChildrenToShape | ItemIsFocusable);
    this->setZValue(parent->zValue() + 1);
}

CurveView::~CurveView()
{
}


QRectF CurveView::boundingRect() const
{
    return m_rect;
}

void CurveView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if(m_selectArea != QRectF{})
    {
        painter->setPen(Qt::white);
        //painter->setCompositionMode(QPainter::CompositionMode_Xor);
        //painter->setPen(QPen{QColor{0, 0, 0, 127}, 2, Qt::DashLine, Qt::SquareCap, Qt::BevelJoin});

        painter->drawRect(m_selectArea);
    }
}

void CurveView::setSelectionArea(const QRectF& rect)
{
    m_selectArea = rect;
    update();
}

void CurveView::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
        emit pressed(event->scenePos());
    event->accept();
}

void CurveView::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    emit moved(event->scenePos());
    event->accept();
}

void CurveView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    emit released(event->scenePos());
    event->accept();
}
#include <QKeyEvent>
void CurveView::keyPressEvent(QKeyEvent* ev)
{
    emit keyPressed(ev->key());
    ev->accept();
}

void CurveView::keyReleaseEvent(QKeyEvent* ev)
{
    emit keyReleased(ev->key());
    ev->accept();
}

void CurveView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
    emit contextMenuRequested(ev->screenPos());
}

void CurveView::setRect(const QRectF& theRect)
{
    prepareGeometryChange();
    m_rect = theRect;
}
