#include "TemporalConstraintView.hpp"

#include "TemporalConstraintViewModel.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QWidget>
#include <QGraphicsProxyWidget>
#include <QPushButton>

TemporalConstraintView::TemporalConstraintView(QGraphicsObject* parent):
	AbstractConstraintView{parent}
{
	this->setParentItem(parent);
	this->setFlag(ItemIsSelectable);

	this->setZValue(parent->zValue() + 1);
}

QRectF TemporalConstraintView::boundingRect() const
{
    return {0, -18, qreal(maxWidth()), qreal(constraintHeight())};
}

void TemporalConstraintView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	QColor c = Qt::black;

	if(isSelected())
	{
		c = Qt::blue;
	}
/*	else if(parentItem()->isSelected())
	{
		c = Qt::cyan;
	}
*/
	if (defaultWidth() < 0)
    {
        c = Qt::red;
    }

	m_solidPen.setColor(c);
	m_dashPen.setColor(c);

	if(minWidth() == maxWidth())
	{
		painter->setPen(m_solidPen);
		painter->drawLine(0,
						  0,
						  defaultWidth(),
						  0);
	}
	else
	{
		// Firs the line going from 0 to the min
		painter->setPen(m_solidPen);
		painter->drawLine(0,
						  0,
						  minWidth(),
						  0);

		// The little hat
		painter->drawLine(minWidth(),
						  -5,
						  minWidth(),
						  -15);
		painter->drawLine(minWidth(),
						  -15,
						  maxWidth(),
						  -15);
		painter->drawLine(maxWidth(),
						  -5,
						  maxWidth(),
						  -15);

		// Finally the dashed line
		painter->setPen(m_dashPen);
		painter->drawLine(minWidth(),
						  0,
						  maxWidth(),
						  0);
	}
	// TODO max -> +inf

}

void TemporalConstraintView::mousePressEvent(QGraphicsSceneMouseEvent* m)
{
	QGraphicsObject::mousePressEvent(m);

	m_clickedPoint = m->pos();
	emit constraintPressed(pos() + m->pos());
}

void TemporalConstraintView::mouseReleaseEvent(QGraphicsSceneMouseEvent *m)
{
	QGraphicsObject::mouseReleaseEvent(m);

	auto posInScenario = pos() + m->pos() - m_clickedPoint;

	if ((m->pos() - m_clickedPoint).x() < 10 && (m->pos() - m_clickedPoint).x() > -10) // @todo use a const !
	{
		posInScenario.setX(pos().x());
	}
	if ((m->pos() - m_clickedPoint).y() < 10 && (m->pos() - m_clickedPoint).y() > -10) // @todo use a const !
	{
		posInScenario.setY(pos().y());
	}
    if(m->pos() != m_clickedPoint) emit constraintReleased(posInScenario);
}
