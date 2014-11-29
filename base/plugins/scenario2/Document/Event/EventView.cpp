#include "EventView.hpp"
#include <QPainter>
#include <QGraphicsScene>


EventView::EventView(QGraphicsObject* parent):
	QNamedGraphicsObject{parent, "EventView"}
{
	this->setParentItem(parent);
	//this->parentItem()->scene()->addItem(this);
	m_rect.setHeight(30);
	m_rect.setWidth(30);
}

QRectF EventView::boundingRect() const
{
	return m_rect;
}

void EventView::paint(QPainter* painter,
					  const QStyleOptionGraphicsItem* option,
					  QWidget* widget)
{
	painter->drawEllipse(m_rect.topLeft(), 15, 15);
}