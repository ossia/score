#include "ScenarioProcessView.hpp"

#include <tools/NamedObject.hpp>

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>

ScenarioProcessView::ScenarioProcessView(QGraphicsObject* parent):
	ProcessViewInterface{parent}
{
	this->setParentItem(parent);
	this->setFlags(ItemClipsChildrenToShape);

	this->setZValue(parent->zValue() + 1);
	//this->parentItem()->scene()->addItem(this);
}


QRectF ScenarioProcessView::boundingRect() const
{
	auto pr = parentItem()->boundingRect();
	return {0, 0,
			pr.width()  - 2 * DEMO_PIXEL_SPACING_TEST,
			pr.height() - 2 * DEMO_PIXEL_SPACING_TEST};
}


void ScenarioProcessView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	painter->drawText(boundingRect(), "Scenario");
	painter->drawRect(boundingRect());
}


void ScenarioProcessView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	if(event->modifiers() == Qt::ControlModifier)  {
		qDebug() << "Scenario: mouse press while ctrl key pressed";
		emit scenarioPressed(event->pos());
	}
}

void ScenarioProcessView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
}

void ScenarioProcessView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	if(event->modifiers() == Qt::ControlModifier) {
		qDebug() << "Scenario: mouse release while ctrl key pressed";
		emit scenarioReleased(event->pos());
	}
}
