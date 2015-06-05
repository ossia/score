#include "CurvePointView.hpp"
#include <QPainter>
#include <iscore/selection/Selectable.hpp>
#include "CurvePointModel.hpp"


// From here we play the role of a presenter...
// TODO see if there would be a better place

#include "CurveTest/UpdateCurve.hpp"
#include "CurveTest/CurvePresenter.hpp"
#include "CurveTest/CurveModel.hpp"
#include "CurveTest/CurveSegmentModel.hpp"
#include "CurveTest/CurvePointModel.hpp"
#include "CurveTest/CurvePointView.hpp"
#include "CurveTest/LinearCurveSegmentModel.hpp"
#include "CurveTest/CurveSegmentModelSerialization.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <QMenu>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsScene>
#include <QGraphicsView>

CurvePointView::CurvePointView(
        CurvePointModel* model,
        QGraphicsItem* parent):
    QGraphicsObject{parent},
    m_model{model}
{
    this->setZValue(2);
    connect(&m_model->selection, &Selectable::changed,
            this, &CurvePointView::setSelected);

    m_contextMenu = new QMenu;

}

CurvePointModel &CurvePointView::model() const
{
    return *m_model;
}

int CurvePointView::type() const
{
    return QGraphicsItem::UserType + 10;
}

QRectF CurvePointView::boundingRect() const
{
    return {-3, -3, 6, 6};
}

void CurvePointView::paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
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

void CurvePointView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
    emit contextMenuRequested(ev->screenPos());
}
