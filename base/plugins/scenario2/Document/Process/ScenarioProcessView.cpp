#include "ScenarioProcessView.hpp"
#include <QPainter>
#include <QGraphicsScene>
#include <QNamedObject>

ScenarioProcessView::ScenarioProcessView(QGraphicsObject* parent):
	iscore::ProcessViewInterface{parent}
{
	this->setParentItem(parent);
	this->setFlags(ItemClipsChildrenToShape);
	//this->parentItem()->scene()->addItem(this);
}


QRectF ScenarioProcessView::boundingRect() const
{
	auto pr = parentItem()->boundingRect();
	return {pr.x() + DEMO_PIXEL_SPACING_TEST,
			pr.y() + DEMO_PIXEL_SPACING_TEST,
			pr.width()  - 2 * DEMO_PIXEL_SPACING_TEST,
			pr.height() - 2 * DEMO_PIXEL_SPACING_TEST};
}


void ScenarioProcessView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	painter->drawText(boundingRect(), "Scenario");
	painter->drawRect(boundingRect());
}
