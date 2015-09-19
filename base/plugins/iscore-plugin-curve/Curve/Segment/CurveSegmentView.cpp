#include "CurveSegmentView.hpp"
#include "CurveSegmentModel.hpp"
#include <QGraphicsSceneContextMenuEvent>
#include <QPainter>
#include <QCursor>
static QColor baseColor{QColor::fromRgb(221, 195, 3)};
CurveSegmentView::CurveSegmentView(
        const CurveSegmentModel& model,
        QGraphicsItem *parent):
    QGraphicsObject{parent},
    m_model{model}
{
    this->setCursor(Qt::ArrowCursor);
    this->setZValue(parent->zValue() + 1);
    this->setFlag(ItemIsFocusable, false);

    con(m_model.selection, &Selectable::changed,
            this, &CurveSegmentView::setSelected);
    con(m_model, &CurveSegmentModel::dataChanged,
            this, &CurveSegmentView::updatePoints);
}

const Id<CurveSegmentModel>& CurveSegmentView::id() const
{
    return m_model.id();
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
    static const QColor base = QColor::fromRgb(0xC7, 0x1F, 0x2C);
    static const QColor yellow = QColor::fromRgb(0xD8, 0xB2, 0x18);

    QPen pen;
    pen.setWidth(m_enabled ? 2 : 1);
    pen.setColor(m_enabled
                    ? (m_selected
                        ? yellow
                        : base)
                    : Qt::gray);

    painter->setPen(pen);
    painter->drawPath(m_shape);
}

void CurveSegmentView::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void CurveSegmentView::enable()
{
    m_enabled = true;
    updateStroke();
    update();
}

void CurveSegmentView::disable()
{
    m_enabled = false;
    updateStroke();
    update();
}


void CurveSegmentView::updatePoints()
{
    // Get the length of the segment to scale.
    double len = m_model.end().x() - m_model.start().x();
    double startx = m_model.start().x() * m_rect.width() / len;
    double scalex = m_rect.width() / len;

    m_model.updateData(25); // Set the number of required points here.
    const auto& pts = m_model.data();

    // Map to the scene coordinates
    if(!pts.empty())
    {
        auto first = pts.first();
        auto first_scaled = QPointF{
                first.x() * scalex - startx,
                (1. - first.y()) * m_rect.height()};

        m_unstrockedShape = QPainterPath{first_scaled};
        for(int i = 1; i < pts.size(); i++)
        {
            const auto& next = pts.at(i);
            m_unstrockedShape.lineTo(QPointF{
                         next.x() * scalex - startx,
                         (1. - next.y()) * m_rect.height()});
        }

        updateStroke();
    }

    update();
}

void CurveSegmentView::updateStroke()
{
    QPainterPathStroker stroker;
    stroker.setWidth(m_enabled ? 0.5 : 0.2);
    m_shape = stroker.createStroke(m_unstrockedShape);
}


QPainterPath CurveSegmentView::shape() const
{
    return m_shape;
}

void CurveSegmentView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
    emit contextMenuRequested(ev->screenPos());
}
