#include "BoxView.hpp"

#include <tools/NamedObject.hpp>

#include <QPainter>
#include <QGraphicsScene>
#include <QDebug>

BoxView::BoxView(QGraphicsObject* parent):
	QGraphicsObject{parent}
{
	this->setParentItem(parent);

	this->setZValue(parent->zValue() + 1);
}

QRectF BoxView::boundingRect() const
{
	return {0,
			0,
			qreal(m_width),
			qreal(m_height)};
}

void BoxView::paint(QPainter* painter,
								const QStyleOptionGraphicsItem* option,
								QWidget* widget)
{
	painter->drawText(boundingRect(), "Box");
	painter->drawRect(boundingRect());
}
