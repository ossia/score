#include "TemporalConstraintView.hpp"

#include "TemporalConstraintViewModel.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QWidget>
#include <QGraphicsProxyWidget>
#include <QPushButton>

TemporalConstraintView::TemporalConstraintView(TemporalConstraintViewModel* viewModel, QGraphicsObject* parent):
	QGraphicsObject{parent},
	m_viewModel{viewModel}
{
	this->setParentItem(parent);
	this->setFlag(ItemIsSelectable);

	this->setZValue(parent->zValue() + 1);
}

QRectF TemporalConstraintView::boundingRect() const
{
    return {0, -18, qreal(m_maxWidth) + 3, qreal(m_height) + 3};
}

void TemporalConstraintView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	QColor c = Qt::black;

	if(isSelected())
	{
		c = Qt::blue;
	}
	else if(parentItem()->isSelected())
	{
		c = Qt::cyan;
	}

    if (m_width < 0)
    {
        c = Qt::red;
    }

	m_solidPen.setColor(c);
	m_dashPen.setColor(c);

    if(m_minWidth == m_maxWidth)
	{
		painter->setPen(m_solidPen);
		painter->drawLine(0,
						  0,
                          m_width,
						  0);
	}
	else
	{
		// Firs the line going from 0 to the min
		painter->setPen(m_solidPen);
		painter->drawLine(0,
						  0,
                          m_minWidth,
						  0);

		// The little hat
        painter->drawLine(m_minWidth,
						  -5,
                          m_minWidth,
						  -15);
        painter->drawLine(m_minWidth,
						  -15,
                          m_maxWidth,
						  -15);
        painter->drawLine(m_maxWidth,
						  -5,
                          m_maxWidth,
						  -15);

		// Finally the dashed line
		painter->setPen(m_dashPen);
        painter->drawLine(m_minWidth,
						  0,
                          m_maxWidth,
						  0);
	}
	// TODO max -> +inf


}

void TemporalConstraintView::setWidth(int width)
{
	prepareGeometryChange();
    m_width = width;
}

void TemporalConstraintView::setMaxWidth(int max)
{
    prepareGeometryChange();
    m_maxWidth = max;
}

void TemporalConstraintView::setMinWidth(int min)
{
    prepareGeometryChange();
    m_minWidth = min;
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
    if(m->pos() != m_clickedPoint) emit constraintReleased(posInScenario);
}
