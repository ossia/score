#include "ConstraintView.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QWidget>
#include <QGraphicsProxyWidget>
#include <QPushButton>

ConstraintView::ConstraintView(QGraphicsObject* parent):
	QGraphicsObject{parent}
{
	this->setParentItem(parent);
	this->setFlag(ItemIsSelectable);

	this->setZValue(parent->zValue() + 1);

	m_button = new QGraphicsProxyWidget(this);
	auto pb = new QPushButton("Add scenario");
	connect(pb,	&QPushButton::clicked,
			[&] ()
		{
			emit addScenarioProcessClicked();
		});

	m_button->setWidget(pb);
}

QRectF ConstraintView::boundingRect() const
{
	return {0, 0, qreal(m_width), qreal(m_height)};
}

void ConstraintView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	if(isSelected())
	{
		painter->setPen(Qt::blue);
	}

	auto rect = boundingRect();
	painter->drawRect(rect);
	painter->drawRect(rect.x(),
					  rect.y(),
					  rect.width(),
					  15);
	painter->drawText(rect, "Constraint");
}

void ConstraintView::setWidth(int width)
{
	prepareGeometryChange();
	m_width = width;
}

void ConstraintView::setHeight(int height)
{
	prepareGeometryChange();
	m_height = height;
}

void ConstraintView::mousePressEvent(QGraphicsSceneMouseEvent* m)
{
	QGraphicsObject::mousePressEvent(m);
	emit constraintPressed(m->pos());
}

void ConstraintView::mouseReleaseEvent(QGraphicsSceneMouseEvent *m)
{
	QGraphicsObject::mouseReleaseEvent(m);
	emit constraintReleased(m->pos());
}
