#include "TemporalScenarioView.hpp"

#include <iscore/tools/NamedObject.hpp>

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QDebug>

TemporalScenarioView::TemporalScenarioView(QGraphicsObject* parent) :
    ProcessViewInterface {parent}
{
    this->setParentItem(parent);
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);

    this->setZValue(parent->zValue() + 1);

    m_clearAction = new QAction("clear contents", this);
    connect(m_clearAction,  &QAction::triggered,
    this,           &TemporalScenarioView::clearPressed);
}

void TemporalScenarioView::paint(QPainter* painter,
                                 const QStyleOptionGraphicsItem* option,
                                 QWidget* widget)
{
    if(m_lock)
    {
        painter->setBrush({Qt::red, Qt::DiagCrossPattern});
        painter->drawRect(boundingRect());
    }

    if(m_selectArea != QRectF{})
    {
        painter->setPen(Qt::black);
        painter->drawRect(m_selectArea);
    }
}


void TemporalScenarioView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsObject::mousePressEvent(event);

    if(event->button() == Qt::LeftButton)
    {
        emit scenarioPressed(event->pos());
        m_clicked = true;
    }
}

void TemporalScenarioView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsObject::mouseMoveEvent(event);
    emit scenarioMoved(event->pos());
}

void TemporalScenarioView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsObject::mouseReleaseEvent(event);

    emit scenarioReleased(event->pos());
}

void TemporalScenarioView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    QMenu contextMenu {};
    contextMenu.clear();
    contextMenu.addAction(m_clearAction);
    contextMenu.exec(event->screenPos());
}
