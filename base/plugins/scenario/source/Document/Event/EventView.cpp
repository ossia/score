#include "EventView.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>

EventView::EventView(QGraphicsObject* parent):
	QGraphicsObject{parent}
{
	this->setParentItem(parent);

	// TODO hack. How to do it properly ? should events be "over" constraints ? maybe +1.5 ?
	this->setZValue(parent->zValue() + 2);

	this->setFlag(ItemIsSelectable);
}

QRectF EventView::boundingRect() const
{
	// TODO inclure les lignes dans le rect
	return {-15, -15, 30, 30};
}

void EventView::paint(QPainter* painter,
					  const QStyleOptionGraphicsItem* option,
					  QWidget* widget)
{
	// Rect
	if(isSelected())
	{
		painter->setPen(Qt::blue);
	}
	else if(parentItem()->isSelected())
	{
		painter->setPen(Qt::cyan);
	}

	painter->drawRect(boundingRect());

	// Ball
	painter->setBrush(QBrush(QColor(80, 140, 50)));
	painter->setPen(QPen(QBrush(QColor(130, 220, 80)), 3, Qt::DashLine));
	painter->drawEllipse(boundingRect().center(), 15, 15);

	painter->setPen(QPen(QBrush(QColor(0,0,0)), 1, Qt::SolidLine));

	// Lines
	painter->drawLine(m_firstLine);
	painter->drawLine(m_secondLine);

}

void EventView::setLinesExtremity(int topPoint, int bottomPoint)
{
	m_firstLine.setP1(boundingRect().center());
    m_firstLine.setP2(QPointF(boundingRect().center().x(), topPoint)); // @todo where does the 80 come from ??

	m_secondLine.setP1(boundingRect().center());
    m_secondLine.setP2(QPointF(boundingRect().center().x(), bottomPoint));
}

void EventView::mousePressEvent(QGraphicsSceneMouseEvent* m)
{
	QGraphicsObject::mousePressEvent(m);

	m_clickedPoint = m->pos();
	emit eventPressed();
}

void EventView::mouseReleaseEvent(QGraphicsSceneMouseEvent* m)
{
	auto posInScenario = pos() + m->pos() - m_clickedPoint;

	if(m->modifiers() == Qt::ControlModifier)
	{
		emit eventReleasedWithControl(posInScenario);
	}
	else
	{
		emit eventReleased(posInScenario);
	}
}

void EventView::mouseMoveEvent(QGraphicsSceneMouseEvent *m)
{

}
