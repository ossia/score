#include "SlotView.hpp"
#include "SlotOverlay.hpp"
#include "SlotHandle.hpp"
#include <iscore/tools/NamedObject.hpp>

#include <QGraphicsScene>
#include <QApplication>
#include <QPainter>
#include <ProcessInterface/Style/ScenarioStyle.hpp>
SlotView::SlotView(const SlotPresenter &pres, QGraphicsObject* parent) :
    QGraphicsObject {parent},
    presenter{pres},
    m_handle{new SlotHandle{*this, this}}
{
    this->setFlag(ItemClipsChildrenToShape, true);
    this->setZValue(parent->zValue() + 1);
    m_handle->setPos(0, this->boundingRect().height() - SlotHandle::handleHeight());
}

QRectF SlotView::boundingRect() const
{
    return {0, 0, m_width, m_height};
}

void SlotView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    const auto& rect = boundingRect();

    painter->setPen(QPen{m_focus? ScenarioStyle::instance().SlotFocus : Qt::transparent});
    painter->drawRect(rect);
}

void SlotView::setHeight(qreal height)
{
    prepareGeometryChange();
    m_height = height;
    m_handle->setPos(0, this->boundingRect().height() - SlotHandle::handleHeight());
    if(m_overlay)
        m_overlay->setHeight(m_height);
}

qreal SlotView::height() const
{
    return m_height;
}

void SlotView::setWidth(qreal width)
{
    prepareGeometryChange();
    m_width = width;
    if(m_overlay)
        m_overlay->setWidth(m_width);
    m_handle->setWidth(width);
}

qreal SlotView::width() const
{
    return m_width;
}

void SlotView::enable()
{
    if(!m_overlay)
        return;

    this->update();

    for(QGraphicsItem* item : childItems())
    {
        item->setEnabled(true);
        item->setFlag(ItemStacksBehindParent, false);
    }

    delete m_overlay;
    m_overlay = nullptr;

    this->update();
}

void SlotView::disable()
{
    delete m_overlay;

    for(QGraphicsItem* item : childItems())
    {
        item->setEnabled(false);
        item->setFlag(ItemStacksBehindParent, true);
    }

    m_overlay = new SlotOverlay{this};
    m_handle->setFlag(ItemStacksBehindParent, false);
    m_handle->setEnabled(true);
}

void SlotView::setFocus(bool b)
{
    m_focus = b;
    update();
}

void SlotView::setFrontProcessName(const QString& s)
{
    m_frontProcessName = s;
}
