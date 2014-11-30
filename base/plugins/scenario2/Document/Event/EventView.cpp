#include "EventView.hpp"
#include <QPainter>
#include <QGraphicsScene>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>

EventView::EventView(QGraphicsObject* parent):
	QNamedGraphicsObject{parent, "EventView"}
{
	this->setParentItem(parent);

	// TODO hack. How to do it properly ? should events be "over" intervals ? maybe +1.5 ?
	this->setZValue(parent->zValue() + 2);
}

QRectF EventView::boundingRect() const
{
	return m_rect;
}

void EventView::paint(QPainter* painter,
					  const QStyleOptionGraphicsItem* option,
					  QWidget* widget)
{
	painter->drawRect(m_rect);
	painter->setBrush(QBrush(QColor(80, 140, 50)));
	painter->setPen(QPen(QBrush(QColor(130, 220, 80)), 3, Qt::DashLine));
	painter->drawEllipse(m_rect.center(), 15, 15);
}

void EventView::setTopLeft(QPointF p)
{
	m_rect = {p.x() - m_rect.width() / 2,
			  p.y() - m_rect.height() / 2,
			  m_rect.width(),
			  m_rect.height()};
}

void EventView::mousePressEvent(QGraphicsSceneMouseEvent* m)
{
	qDebug() <<"Event clicked";
}