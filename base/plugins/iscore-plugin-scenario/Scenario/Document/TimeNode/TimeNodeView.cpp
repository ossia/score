#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QGraphicsSceneEvent>
#include <qnamespace.h>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <QGraphicsScene>

#include <iscore/model/ModelMetadata.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include "TimeNodePresenter.hpp"
#include "TimeNodeView.hpp"
#include <QCursor>

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
TimeNodeView::TimeNodeView(TimeNodePresenter& presenter,
                           QGraphicsItem* parent) :
    QGraphicsItem {parent},
    m_presenter{presenter}
{
    this->setCacheMode(QGraphicsItem::NoCache);
    this->setParentItem(parent);
    this->setZValue(ZPos::TimeNode);
    this->setAcceptHoverEvents(true);
    this->setCursor(Qt::CrossCursor);

    auto& skin = iscore::Skin::instance();

    m_text = new SimpleTextItem{this};
    m_color = presenter.model().metadata().getColor();

    auto f = skin.SansFont;
    f.setPointSize(10);
    m_text->setFont(f);
    m_text->setColor(iscore::ColorRef{&skin.Gray});
}

TimeNodeView::~TimeNodeView()
{
}

void TimeNodeView::paint(QPainter* painter,
                         const QStyleOptionGraphicsItem* option,
                         QWidget* widget)
{
    auto& skin = ScenarioStyle::instance();
    painter->setRenderHint(QPainter::Antialiasing, false);
    QColor pen_color;
    if(isSelected())
    {
        pen_color = skin.TimenodeSelected.getColor();
    }
    else
    {
        pen_color = m_color.getColor();
    }

    skin.TimenodeBrush.setColor(pen_color);
    skin.TimenodePen.setBrush(skin.TimenodeBrush);
    painter->setPen(skin.TimenodePen);

    painter->fillRect(QRectF(QPointF(-1, 0), QPointF(1, m_extent.bottom() - m_extent.top())),
                      skin.TimenodeBrush);

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
    painter->setPen(Qt::darkMagenta);
    painter->drawRect(boundingRect());
#endif
}

void TimeNodeView::setExtent(const VerticalExtent& extent)
{
    prepareGeometryChange();
    m_extent = extent;
    this->update();
}

void TimeNodeView::setExtent(VerticalExtent &&extent)
{
    prepareGeometryChange();
    m_extent = std::move(extent);
    this->update();
}

void TimeNodeView::setMoving(bool arg)
{
    update();
}

void TimeNodeView::setTriggerActive(bool b)
{
    if(b)
        m_text->setPos(-m_text->boundingRect().width() / 2, -40);
    else
        m_text->setPos(-m_text->boundingRect().width() / 2, -20);
}

void TimeNodeView::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void TimeNodeView::changeColor(iscore::ColorRef newColor)
{
    m_color = newColor;
    this->update();
}

void TimeNodeView::setLabel(const QString& s)
{
    m_text->setText(s);

    // Used to re-set the text position
    setTriggerActive(m_text->pos().y() == -40);
}


void TimeNodeView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->button() == Qt::MouseButton::LeftButton)
        emit m_presenter.pressed(event->scenePos());
}

void TimeNodeView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.moved(event->scenePos());
}

void TimeNodeView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.released(event->scenePos());
}
}
