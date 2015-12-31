#include <Curve/CurveStyle.hpp>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPen>
#include <cstddef>
#include <vector>

#include <Curve/Palette/CurvePoint.hpp>
#include "CurveSegmentModel.hpp"
#include "CurveSegmentView.hpp"
#include <iscore/selection/Selectable.hpp>
#include <iscore/tools/Todo.hpp>

class QStyleOptionGraphicsItem;
class QWidget;
#include <iscore/tools/SettableIdentifier.hpp>
namespace Curve
{
static const QPainterPathStroker CurveSegmentStroker{
    [] () {
        QPen p;
        p.setWidth(12);
        return p;
    }()
};
SegmentView::SegmentView(
        const SegmentModel* model,
        const Curve::Style& style,
        QGraphicsItem *parent):
    QGraphicsObject{parent},
    m_style{style}
{
    this->setZValue(1);
    this->setFlag(ItemIsFocusable, false);

    setModel(model);
}

void SegmentView::setModel(const SegmentModel* model)
{
    m_model = model;

    if(m_model)
    {
        con(m_model->selection, &Selectable::changed,
            this, &SegmentView::setSelected);
        connect(m_model, &SegmentModel::dataChanged,
            this, &SegmentView::updatePoints);
    }
}

const Id<SegmentModel>& SegmentView::id() const
{
    return m_model->id();
}

void SegmentView::setRect(const QRectF& theRect)
{
    prepareGeometryChange();
    m_rect = theRect;
    updatePoints();
}

QRectF SegmentView::boundingRect() const
{
    return m_rect;
}

void SegmentView::paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
    QPen pen;
    pen.setWidth(m_enabled ? 2 : 1);
    pen.setColor(m_enabled
                    ? (m_selected
                        ? m_style.SegmentSelected
                        : m_style.Segment)
                    : m_style.SegmentDisabled);

    painter->setPen(pen);
    painter->drawPath(m_unstrokedShape);

    //painter->fillPath(m_strokedShape, QBrush{Qt::blue});
}


void SegmentView::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void SegmentView::enable()
{
    m_enabled = true;
    update();
}

void SegmentView::disable()
{
    m_enabled = false;
    update();
}

void SegmentView::updatePoints()
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

        m_unstrokedShape = QPainterPath{first_scaled};
        for(std::size_t i = 1; i < pts.size(); i++)
        {
            auto next = pts.at(i);
            m_unstrokedShape.lineTo(QPointF{
                                         next.x() * scalex - startx,
                                         (1. - next.y()) * m_rect.height()});
        }
    }

    m_strokedShape = CurveSegmentStroker.createStroke(m_unstrokedShape);

    update();
}




void SegmentView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
    emit contextMenuRequested(ev->screenPos(), ev->scenePos());
}
}
