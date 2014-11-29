#include "EventView.hpp"
#include <QPainter>
#include <QGraphicsScene>
#include <QDebug>

EventView::EventView(QGraphicsObject* parent):
	QNamedGraphicsObject{parent, "EventView"}
{
	this->setParentItem(parent);
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

void EventView::setTopLeft(QPointF p)
{
	m_rect = {p.x(), p.y(), m_rect.width(), m_rect.height()};
}