#include "TemporalScenarioView.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>

TemporalScenarioView::TemporalScenarioView(QGraphicsObject* parent) :
    ProcessViewInterface {parent}
{
    this->setParentItem(parent);
    this->setFlags(ItemClipsChildrenToShape);// | ItemIsSelectable | ItemIsFocusable);

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
    if(event->button() == Qt::LeftButton)
        emit scenarioPressed(event->scenePos());
}

void TemporalScenarioView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit scenarioMoved(event->scenePos());
}

void TemporalScenarioView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit scenarioReleased(event->scenePos());
}

void TemporalScenarioView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    QMenu contextMenu {};
    contextMenu.clear();
    contextMenu.addAction(m_clearAction);
    contextMenu.exec(event->screenPos());
}

void TemporalScenarioView::keyPressEvent(QKeyEvent* event)
{
    QGraphicsObject::keyPressEvent(event);
    if(event->key() == Qt::Key_Escape)
    {
        emit escPressed();
    }
}
