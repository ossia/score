#include "BaseConstraintView.hpp"

#include "BaseConstraintViewModel.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QWidget>
#include <QGraphicsProxyWidget>
#include <QPushButton>

BaseConstraintView::BaseConstraintView(BaseConstraintViewModel* viewModel, QGraphicsObject* parent):
	QGraphicsObject{parent},
	m_viewModel{viewModel}
{
	this->setParentItem(parent);
	this->setFlag(ItemIsSelectable);

	this->setZValue(parent->zValue() + 1);
}

QRectF BaseConstraintView::boundingRect() const
{
	return {0, -18, qreal(m_width) + 3, qreal(m_height) + 3};
}

void BaseConstraintView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	QColor c = Qt::black;

	m_solidPen.setColor(c);
	m_dashPen.setColor(c);

	painter->setPen(m_solidPen);
	painter->drawLine(0,
					  0,
					  m_width,
					  0);
}

void BaseConstraintView::setWidth(int width)
{
	prepareGeometryChange();
    m_width = width;
}

void BaseConstraintView::setHeight(int height)
{
	prepareGeometryChange();
	m_height = height;
}

void BaseConstraintView::mousePressEvent(QGraphicsSceneMouseEvent* m)
{
	QGraphicsObject::mousePressEvent(m);

	emit constraintPressed(pos() + m->pos());
}
