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

const QPen TemporalConstraintView::m_solidPen = QPen{
													QBrush{Qt::black},
													4,
													Qt::SolidLine,
													Qt::RoundCap,
													Qt::RoundJoin};

const QPen TemporalConstraintView::m_dashPen = QPen{
												   QBrush{Qt::black},
												   4,
												   Qt::DashLine,
												   Qt::RoundCap,
												   Qt::RoundJoin};
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
	return {0, -18, qreal(m_viewModel->model()->maxDuration()) + 3, qreal(m_height) + 3};
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

	auto model = m_viewModel->model();
	if(model->minDuration() == model->maxDuration())
	{
		painter->setPen(m_solidPen);
		painter->drawLine(0,
						  0,
						  model->defaultDuration(),
						  0);
	}
	else
	{
		// Firs the line going from 0 to the min
		painter->setPen(m_solidPen);
		painter->drawLine(0,
						  0,
						  model->minDuration(),
						  0);

		// The little hat
		painter->drawLine(model->minDuration(),
						  -5,
						  model->minDuration(),
						  -15);
		painter->drawLine(model->minDuration(),
						  -15,
						  model->maxDuration(),
						  -15);
		painter->drawLine(model->maxDuration(),
						  -5,
						  model->maxDuration(),
						  -15);

		// Finally the dashed line
		painter->setPen(m_dashPen);
		painter->drawLine(model->minDuration(),
						  0,
						  model->maxDuration(),
						  0);
	}
	// TODO max -> +inf


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
