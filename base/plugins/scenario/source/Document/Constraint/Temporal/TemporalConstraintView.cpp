#include "TemporalConstraintView.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QWidget>
#include <QGraphicsProxyWidget>
#include <QPushButton>

TemporalConstraintView::TemporalConstraintView(QGraphicsObject* parent):
	QGraphicsObject{parent}
{
	this->setParentItem(parent);
	this->setFlag(ItemIsSelectable);

	this->setZValue(parent->zValue() + 1);
}

QRectF TemporalConstraintView::boundingRect() const
{
	return {0, 0, qreal(m_width), qreal(m_height)};
}

void TemporalConstraintView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	if(isSelected())
	{
		painter->setPen(Qt::blue);
	}
	else if(parentItem()->isSelected())
	{
		painter->setPen(Qt::cyan);
	}

	auto rect = boundingRect();
	painter->drawRect(rect);
	painter->drawRect(rect.x(),
					  rect.y(),
					  rect.width(),
					  15);
	painter->drawText(rect, "Constraint");
}

void TemporalConstraintView::setWidth(int width)
{
	prepareGeometryChange();
	m_width = width;
}

void TemporalConstraintView::setHeight(int height)
{
	prepareGeometryChange();
	m_height = height;
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
    emit constraintReleased(posInScenario);
}
