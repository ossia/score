#include "CurveSegmentView.hpp"
#include "CurveSegmentModel.hpp"
#include <QGraphicsSceneContextMenuEvent>
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
    pen.setWidth(2);
    pen.setColor(c);
    painter->setPen(pen);
    painter->drawPath(m_shape);
    //painter->drawLines(m_lines);
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

    m_model->updateData(25); // Set the number of required points here.
    auto pts = m_model->data();

    //m_lines.clear();
    // Map to the scene coordinates
    if(!m_model->data().empty())
    {
        //m_lines.resize(m_model->data().size());
        auto first = m_model->data().first();
        auto first_scaled = QPointF{
                first.x() * scalex - startx,
                (1. - first.y()) * m_rect.height()};
        QPainterPath p{first_scaled};
        for(int i = 1; i < m_model->data().size(); i++)
        {
            const auto& next = m_model->data().at(i);
            auto next_scaled = QPointF{
                    next.x() * scalex - startx,
                    (1. - next.y()) * m_rect.height()};
            p.lineTo(next_scaled);

            //m_lines[i] = QLineF(first_scaled, next_scaled);
            //first_scaled = next_scaled;
        }

        QPainterPathStroker stroker;
        stroker.setWidth(2);
        m_shape = stroker.createStroke(p);
    }

    update();
}


QPainterPath CurveSegmentView::shape() const
{
    return m_shape;
}

void CurveSegmentView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
    emit contextMenuRequested(ev->screenPos());
}
