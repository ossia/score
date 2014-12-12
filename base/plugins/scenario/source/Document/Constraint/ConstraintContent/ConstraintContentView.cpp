#include "ConstraintContentView.hpp"

#include <tools/NamedObject.hpp>

#include <QPainter>
#include <QGraphicsScene>
#include <QDebug>

ConstraintContentView::ConstraintContentView(QGraphicsObject* parent):
	QGraphicsObject{parent}
{
	this->setParentItem(parent);

	this->setZValue(parent->zValue() + 1);
	//parentItem()->scene()->addItem(this);
}

QRectF ConstraintContentView::boundingRect() const
{
	return {parentItem()->boundingRect().x() + DEMO_PIXEL_SPACING_TEST,
			parentItem()->boundingRect().y() + 50 + DEMO_PIXEL_SPACING_TEST,
			parentItem()->boundingRect().width()  - 2 * DEMO_PIXEL_SPACING_TEST,
			parentItem()->boundingRect().height() - 50 - 2 * DEMO_PIXEL_SPACING_TEST};
}

void ConstraintContentView::paint(QPainter* painter,
								const QStyleOptionGraphicsItem* option,
								QWidget* widget)
{
	painter->drawText(boundingRect(), "Box");
	painter->drawRect(boundingRect());
}
