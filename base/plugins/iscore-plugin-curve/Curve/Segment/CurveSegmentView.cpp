#include <Curve/CurveStyle.hpp>
#include <qgraphicssceneevent.h>
#include <qpainter.h>
#include <qpen.h>
#include <cstddef>
#include <vector>

#include "Curve/Palette/CurvePoint.hpp"
#include "CurveSegmentModel.hpp"
#include "CurveSegmentView.hpp"
#include "iscore/selection/Selectable.hpp"
#include "iscore/tools/Todo.hpp"

class QStyleOptionGraphicsItem;
class QWidget;
template <typename tag, typename impl> class id_base_t;

static const QPainterPathStroker CurveSegmentStroker{
    [] () {
        QPen p;
        p.setWidth(12);
        return p;
    }()
};
CurveSegmentView::CurveSegmentView(
        const CurveSegmentModel* model,
        const Curve::Style& style,
        QGraphicsItem *parent):
    QGraphicsObject{parent},
    m_style{style}
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




void CurveSegmentView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
    emit contextMenuRequested(ev->screenPos(), ev->scenePos());
}
