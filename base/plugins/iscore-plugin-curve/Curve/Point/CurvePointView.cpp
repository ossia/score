#include "CurvePointView.hpp"
#include <QPainter>
#include <iscore/selection/Selectable.hpp>
#include <iscore/widgets/ClearLayout.hpp>
#include "CurvePointModel.hpp"
#include <Curve/CurveStyle.hpp>
#include <QGraphicsSceneContextMenuEvent>
#include <QCursor>

static const qreal radius = 2.5;
CurvePointView::CurvePointView(
        const CurvePointModel* model,
        const CurveStyle& style,
        QGraphicsItem* parent):
    QGraphicsObject{parent},
    m_style{style}
{
    this->setZValue(2);
    this->setCursor(Qt::CrossCursor);

    setModel(model);
}

void CurvePointView::setModel(const CurvePointModel* model)
{
    m_model = model;
    if(m_model)
    {
        con(m_model->selection, &Selectable::changed,
            this, &CurvePointView::setSelected);
    }
}

const CurvePointModel& CurvePointView::model() const
{
    return *m_model;
}

const Id<CurvePointModel>& CurvePointView::id() const
{
    return m_model->id();
}

QRectF CurvePointView::boundingRect() const
{
    return {-radius, -radius, 2 * radius, 2 * radius};
}

void CurvePointView::paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
    if(!m_enabled)
        return;

    QPen pen;
    QColor c = m_selected
               ? m_style.PointSelected
               : m_style.Point;

    pen.setColor(c);
    pen.setWidth(1);
    painter->setPen(pen);
    painter->setBrush(c);

    pen.setCosmetic(true);
    pen.setWidth(1);

    painter->setPen(pen);
    painter->drawEllipse(boundingRect());
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
    emit contextMenuRequested(ev->screenPos(), ev->scenePos());
}
