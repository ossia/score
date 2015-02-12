#include "AutomationView.hpp"

#include <QPainter>

AutomationView::AutomationView(QGraphicsObject* parent):
	ProcessViewInterface{parent}
{
	setZValue(parent->zValue() + 1);
}

QRectF AutomationView::boundingRect() const
{
	auto pr = parentItem()->boundingRect();
	return {0, 0,
		pr.width(),
				pr.height()};
}


void AutomationView::paint(QPainter* painter,
						   const QStyleOptionGraphicsItem* option,
						   QWidget* widget)
{

}
