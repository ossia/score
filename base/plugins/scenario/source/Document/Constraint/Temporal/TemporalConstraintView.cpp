#include "TemporalConstraintView.hpp"

#include "TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QWidget>
#include <QGraphicsProxyWidget>
#include <QPushButton>

// TODO don't use model here, in case it is removed.
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
	auto model = m_viewModel->model();
	if(model->minDuration() == model->maxDuration())
	{
		QPen p{QBrush{Qt::black},
			   4,
			   Qt::SolidLine,
			   Qt::RoundCap,
			   Qt::RoundJoin};

		painter->setPen(p);
		painter->drawLine(rect.x(),
						  rect.y(),
						  rect.x() + model->defaultDuration(),
						  rect.y());
	}
	else
	{
		// Firs the line going from 0 to the min
		QPen solidPen{QBrush{Qt::black},
					  4,
					  Qt::SolidLine,
					  Qt::RoundCap,
					  Qt::RoundJoin};

		painter->setPen(solidPen);
		painter->drawLine(rect.x(),
						  rect.y(),
						  rect.x() + model->minDuration(),
						  rect.y());

		// The little hat
		painter->drawLine(rect.x() + model->minDuration(),
						  rect.y() - 5,
						  rect.x() + model->minDuration(),
						  rect.y() - 15);
		painter->drawLine(rect.x() + model->minDuration(),
						  rect.y() - 15,
						  rect.x() + model->maxDuration(),
						  rect.y() - 15);
		painter->drawLine(rect.x() + model->maxDuration(),
						  rect.y() - 5,
						  rect.x() + model->maxDuration(),
						  rect.y() - 15);

		// Finally the dashed line
		QPen dashPen{QBrush{Qt::black},
					 4,
					 Qt::DashLine,
					 Qt::RoundCap,
					 Qt::RoundJoin};

		painter->setPen(dashPen);
		painter->drawLine(rect.x() + model->minDuration(),
						  rect.y(),
						  rect.x() + model->maxDuration(),
						  rect.y());
	}
	// TODO max -> +inf
	/*
	painter->drawRect(rect);
	painter->drawRect(rect.x(),
					  rect.y(),
					  rect.width(),
					  15);
	painter->drawText(rect, "Constraint");
	*/


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
