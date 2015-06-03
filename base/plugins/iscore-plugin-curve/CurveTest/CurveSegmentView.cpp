#include "CurveSegmentView.hpp"
#include "CurveSegmentModel.hpp"
#include <QPainter>

CurveSegmentView::CurveSegmentView(CurveSegmentModel *model, QGraphicsItem *parent):
    QGraphicsObject{parent},
    m_model{model}
{
    this->setZValue(1);

    connect(&m_model->selection, &Selectable::changed,
            this, &CurveSegmentView::setSelected);
    connect(m_model, &CurveSegmentModel::dataChanged,
            this, &CurveSegmentView::updatePoints);
}

int CurveSegmentView::type() const
{
    return QGraphicsItem::UserType + 11;
}

void CurveSegmentView::setRect(const QRectF& theRect)
{
    prepareGeometryChange();
    m_rect = theRect;
    updatePoints();
}

QRectF CurveSegmentView::boundingRect() const
{
    return m_rect;
}

void CurveSegmentView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QColor c = m_selected ? Qt::yellow : Qt::red;
    QPen pen;
    pen.setColor(c);
    painter->setPen(pen);
    painter->fillPath(m_shape, c);
}

void CurveSegmentView::setSelected(bool selected)
{
    m_selected = selected;
    update();
}


void CurveSegmentView::updatePoints()
{
    // Get the length of the segment to scale.
    double len = m_model->end().x() - m_model->start().x();
    double startx = m_model->start().x() * m_rect.width() / len;
    double scalex = m_rect.width() / len;

    auto pts = m_model->data(25); // Set the number of required points here.

    // Map to the scene coordinates
    std::transform(pts.begin(), pts.end(), pts.begin(),
                   [&] (const QPointF& pt) {
        return QPointF{
            pt.x() * scalex - startx,
            (1. - pt.y()) * m_rect.height()};
    });

    m_points = std::move(pts);

    if(!m_points.empty())
    {
        QPainterPath p(m_points.first());

        for(int i = 0; i < m_points.size(); i++)
            p.lineTo(m_points[i]);

        QPainterPathStroker stroker;
        stroker.setWidth(3);
        m_shape = stroker.createStroke(p);
    }

    update();
}


QPainterPath CurveSegmentView::shape() const
{
    return m_shape;
}
