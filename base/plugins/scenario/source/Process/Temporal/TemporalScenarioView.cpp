#include "TemporalScenarioView.hpp"

#include <public_interface/tools/NamedObject.hpp>

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
    if(isSelected())
    {
        painter->setPen(Qt::blue);
    }

    painter->drawRect(boundingRect());

    if(m_lock)
    {
        painter->setBrush({Qt::red, Qt::DiagCrossPattern});
        painter->drawRect(boundingRect());
    }

    if(m_clicked)
    {
        painter->setPen(Qt::black);
        painter->drawRect(*m_selectArea);
    }
}


void TemporalScenarioView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsObject::mousePressEvent(event);

    if(event->modifiers() == Qt::ControlModifier)
    {
        emit scenarioPressedWithControl(event->pos(), event->scenePos());
    }
    else if(event->button() == Qt::LeftButton)
    {
        emit scenarioPressed();
        m_selectArea = new QRectF {0, 0, 0, 0};
        m_clickedPoint = event->pos();
        m_clicked = true;
    }
}

void TemporalScenarioView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsObject::mouseMoveEvent(event);

    if(m_clicked)
    {
        m_selectArea->setTopLeft(m_clickedPoint);
        m_selectArea->setBottomRight(event->pos());
    }

    this->update();
}

void TemporalScenarioView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsObject::mouseReleaseEvent(event);

    if(event->modifiers() == Qt::ControlModifier)
    {
        emit scenarioReleased(event->pos(), mapToScene(event->pos()));
    }
    else
    {
        QPainterPath path {};
        QRectF rect {};
        rect.setTopLeft(this->mapToScene(m_clickedPoint));
        rect.setBottomRight(event->scenePos());

        delete m_selectArea;
        this->update();
        m_clicked = false;
        emit newSelectionArea(rect);
    }
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
    if(event->key() == Qt::Key_Delete)
    {
        emit deletePressed();
    }
    else if(event->key() == Qt::Key_Control)
    {
        emit ctrlStateChanged(true);
    }
}

void TemporalScenarioView::keyReleaseEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Control)
    {
        emit ctrlStateChanged(false);
    }
}
