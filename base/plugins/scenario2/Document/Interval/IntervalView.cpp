#include "IntervalView.hpp"
#include <QPainter>
#include <QGraphicsScene>
#include <QDebug>
IntervalView::IntervalView(QGraphicsObject* parent):
	QNamedGraphicsObject{parent, "IntervalView"}
{
	this->setParentItem(parent);
	//this->parentItem()->scene()->addItem(this);
}

QRectF IntervalView::boundingRect() const
{
	return m_rect;
}

void IntervalView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	qDebug() << "Interval rect: " << m_rect;
	painter->drawRect(m_rect);
	painter->drawRect(m_rect.x(),
					  m_rect.y(),
					  m_rect.width(),
					  15);
	painter->drawText(m_rect, "Interval");
}
