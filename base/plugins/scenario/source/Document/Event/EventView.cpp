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
    this->setAcceptHoverEvents(true);
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
	QColor pen_color = Qt::black;
	// Rect
	if(isSelected())
	{
		pen_color = Qt::blue;
	}
	else if(parentItem()->isSelected())
	{
		pen_color = Qt::cyan;
	}

	//painter->drawRect(boundingRect());

	// Ball
	painter->setBrush(pen_color);
	painter->setPen(pen_color);
	painter->drawEllipse(boundingRect().center(), 5, 5);

	painter->setPen(QPen(QBrush(QColor(0,0,0)), 1, Qt::SolidLine));

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
        emit eventReleasedWithControl(posInScenario, mapToScene(m->pos()));
	}
    else
    {   // @todo : aimantation à revoir.
        if ((m->pos() - m_clickedPoint).x() < 10 && (m->pos() - m_clickedPoint).x() > -10) // @todo use a const !
        {
            posInScenario.setX(pos().x());
        }
        if ((m->pos() - m_clickedPoint).y() < 10 && (m->pos() - m_clickedPoint).y() > -10) // @todo use a const !
        {
            posInScenario.setY(pos().y());
        }
        emit eventReleased(posInScenario);
	}
}

void EventView::mouseMoveEvent(QGraphicsSceneMouseEvent *m)
{

}
