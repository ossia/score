#include "CurveSegmentView.hpp"
#include "CurveSegmentModel.hpp"
#include <QGraphicsSceneContextMenuEvent>
#include <QPainter>
#include <Curve/CurveStyle.hpp>

#include <QCursor>
CurveSegmentView::CurveSegmentView(
        const CurveSegmentModel* model,
        QGraphicsItem *parent):
    QGraphicsObject{parent}
{
    this->setZValue(1);
    this->setFlag(ItemIsFocusable, false);

    setModel(model);
}

void CurveSegmentView::setModel(const CurveSegmentModel* model)
{
    m_model = model;

    if(m_model)
    {
        con(m_model->selection, &Selectable::changed,
            this, &CurveSegmentView::setSelected);
        connect(m_model, &CurveSegmentModel::dataChanged,
            this, &CurveSegmentView::updatePoints);
    }
}

const Id<CurveSegmentModel>& CurveSegmentView::id() const
{
    return m_model->id();
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

void CurveSegmentView::paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
    auto& style = CurveStyle::instance();
    QPen pen;
    pen.setWidth(m_enabled ? 2 : 1);
    pen.setColor(m_enabled
                    ? (m_selected
                        ? style.SegmentSelected
                        : style.Segment)
                    : style.SegmentDisabled);

    painter->setPen(pen);
    painter->drawPath(m_unstrockedShape);
}


void CurveSegmentView::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void CurveSegmentView::enable()
{
    m_enabled = true;
    update();
}

void CurveSegmentView::disable()
{
    m_enabled = false;
    update();
}

void CurveSegmentView::updatePoints()
{
    if(!m_model)
        return;

    // Get the length of the segment to scale.
    double len = m_model->end().x() - m_model->start().x();
    double startx = m_model->start().x() * m_rect.width() / len;
    double scalex = m_rect.width() / len;

    m_model->updateData(25); // Set the number of required points here.
    const auto& pts = m_model->data();

    // Map to the scene coordinates
    if(!pts.empty())
    {
        auto first = pts.front();
        auto first_scaled = QPointF{
                first.x() * scalex - startx,
                (1. - first.y()) * m_rect.height()};

        m_unstrockedShape = QPainterPath{first_scaled};
        for(std::size_t i = 1; i < pts.size(); i++)
        {
            auto next = pts.at(i);
            m_unstrockedShape.lineTo(QPointF{
                                         next.x() * scalex - startx,
                                         (1. - next.y()) * m_rect.height()});
        }
    }

    update();
}


QPainterPath CurveSegmentView::shape() const
{
    return m_unstrockedShape;
}

void CurveSegmentView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
    emit contextMenuRequested(ev->screenPos());
}
