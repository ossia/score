#include "StoreyView.hpp"

#include <tools/NamedObject.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

StoreyView::StoreyView(QGraphicsObject* parent):
	QGraphicsObject{parent}
{
	this->setParentItem(parent);

	this->setZValue(parent->zValue() + 1);
}

QRectF StoreyView::boundingRect() const
{
	return {0, 0,
			parentItem()->boundingRect().width() - 2 * DEMO_PIXEL_SPACING_TEST,
			qreal(m_height)};
}

void StoreyView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	painter->drawRect(boundingRect());
}

void StoreyView::setHeight(int height)
{
	prepareGeometryChange();
	m_height = height;
}

int StoreyView::height() const
{
	return m_height;
}

void StoreyView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	qDebug(Q_FUNC_INFO);
	emit bottomHandleSelected();
}

void StoreyView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	emit bottomHandleChanged(event->pos().y() - boundingRect().y());
}

void StoreyView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	emit bottomHandleReleased();
}

