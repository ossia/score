#include "StoreyView.hpp"
#include <QGraphicsScene>
#include <QPainter>
StoreyView::StoreyView(QGraphicsObject* parent):
	QNamedGraphicsObject{parent, "StoreyView"}
{
	this->setParentItem(parent);

	this->setZValue(parent->zValue() + 1);
	//parentItem()->scene()->addItem(this);
}

QRectF StoreyView::boundingRect() const
{
	return {parentItem()->boundingRect().x() + DEMO_PIXEL_SPACING_TEST,
			parentItem()->boundingRect().y() + 20,
			parentItem()->boundingRect().width() - 2 * DEMO_PIXEL_SPACING_TEST,
			m_height};
}

void StoreyView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	painter->drawRect(boundingRect());
}
