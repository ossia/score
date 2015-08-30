#include "CurvePointView.hpp"
#include <QPainter>
#include <iscore/selection/Selectable.hpp>
#include "CurvePointModel.hpp"

#include <QGraphicsSceneContextMenuEvent>
#include <QCursor>

CurvePointView::CurvePointView(
        const CurvePointModel& model,
        QGraphicsItem* parent):
    QGraphicsObject{parent},
    m_model{model}
{
    this->setAcceptHoverEvents(true);
    this->setZValue(parent->zValue() + 2);
    con(m_model.selection, &Selectable::changed,
            this, &CurvePointView::setSelected);
}

const CurvePointModel& CurvePointView::model() const
{
    return m_model;
}

const Id<CurvePointModel>& CurvePointView::id() const
{
    return m_model.id();
}

int CurvePointView::type() const
{
    return QGraphicsItem::UserType + 10;
}

QRectF CurvePointView::boundingRect() const
{
    return {-2.5, -2.5, 5., 5.};
}

void CurvePointView::paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
    if(!m_enabled)
        return;

    QPen pen;
    QColor c = m_selected? Qt::yellow : Qt::green;
    pen.setColor(c);
    pen.setWidth(3);
    painter->setPen(pen);
    painter->setBrush(c);

    painter->drawEllipse(QPointF{0., 0.}, 3, 3);
}

void CurvePointView::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void CurvePointView::enable()
{
    m_enabled = true;
    update();
}

void CurvePointView::disable()
{
    m_enabled = false;
    update();
}

void CurvePointView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
    emit contextMenuRequested(ev->screenPos());
}

#include <QApplication>
#include <QWidget>
void CurvePointView::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    qApp->activeWindow()->setCursor(Qt::CrossCursor);
}

void CurvePointView::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    qApp->activeWindow()->setCursor(Qt::ArrowCursor);
}
