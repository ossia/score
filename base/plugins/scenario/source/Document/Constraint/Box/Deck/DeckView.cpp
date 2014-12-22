#include "DeckView.hpp"

#include <tools/NamedObject.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

DeckView::DeckView(QGraphicsObject* parent):
	QGraphicsObject{parent}
{
	this->setParentItem(parent);

	this->setZValue(parent->zValue() + 1);
}

QRectF DeckView::boundingRect() const
{
	return {0, 0,
			parentItem()->boundingRect().width() - 2 * DEMO_PIXEL_SPACING_TEST,
			qreal(m_height)};
}

void DeckView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	painter->drawRect(boundingRect());
}

void DeckView::setHeight(int height)
{
	prepareGeometryChange();
	m_height = height;
}

int DeckView::height() const
{
	return m_height;
}

void DeckView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	emit bottomHandleSelected();
}

void DeckView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	emit bottomHandleChanged(event->pos().y() - boundingRect().y());
}

void DeckView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	emit bottomHandleReleased();
}

