#include "TemporalScenarioProcessView.hpp"

#include <tools/NamedObject.hpp>

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QDebug>

TemporalScenarioProcessView::TemporalScenarioProcessView(QGraphicsObject* parent):
	ProcessViewInterface{parent}
{
	this->setParentItem(parent);
	this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);

	this->setZValue(parent->zValue() + 1);
	//this->parentItem()->scene()->addItem(this);

    m_clearAction = new QAction("clear contents", this);
    connect(m_clearAction,  &QAction::triggered,
            this,           &TemporalScenarioProcessView::clearPressed);
}


QRectF TemporalScenarioProcessView::boundingRect() const
{
	auto pr = parentItem()->boundingRect();

    if (pr.isValid()) 	return {0, 0,
			pr.width()  - 2 * DEMO_PIXEL_SPACING_TEST,
			pr.height() - 2 * DEMO_PIXEL_SPACING_TEST};
    else
        return {0,0,1,1};
}


void TemporalScenarioProcessView::paint(QPainter* painter,
								const QStyleOptionGraphicsItem* option,
								QWidget* widget)
{
	painter->drawText(boundingRect(), "Scenario");

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

    if (m_clicked)
    {
        painter->setPen(Qt::black);
        painter->drawRect(*m_selectArea);
    }
}


void TemporalScenarioProcessView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	QGraphicsObject::mousePressEvent(event);

	if(event->modifiers() == Qt::ControlModifier)
	{
        emit scenarioPressedWithControl(event->pos(), event->scenePos());
	}
    else if (event->button() == Qt::LeftButton)
	{
		emit scenarioPressed();
        m_selectArea = new QRectF{0,0,0,0};
        m_clickedPoint = event->pos();
        m_clicked = true;
	}
}

void TemporalScenarioProcessView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsObject::mouseMoveEvent(event);

    if(m_clicked)
    {
        m_selectArea->setTopLeft(m_clickedPoint);
        m_selectArea->setBottomRight(event->pos());
    }
    this->update();
}

void TemporalScenarioProcessView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	QGraphicsObject::mouseReleaseEvent(event);
	if(event->modifiers() == Qt::ControlModifier)
	{
        emit scenarioReleased(event->pos(), mapToScene(event->pos()));
    }
    else
    {
        QPainterPath path{};
        QRectF rect{};
        rect.setTopLeft(this->mapToScene(m_clickedPoint));
        rect.setBottomRight(event->scenePos());

        path.addRect(rect);
        this->scene()->setSelectionArea(path, QTransform());
        delete m_selectArea;
        this->update();
        m_clicked = false;
    }
}

void TemporalScenarioProcessView::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu contextMenu{};
    contextMenu.clear();
    contextMenu.addAction(m_clearAction);
    contextMenu.exec(event->screenPos());
}

void TemporalScenarioProcessView::keyPressEvent(QKeyEvent* event)
{
	if(event->key() == Qt::Key_Delete)
	{
		emit deletePressed();
	}
}


