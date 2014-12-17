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
	return m_rect;
}

void ConstraintView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	painter->drawRect(m_rect);
	painter->drawRect(m_rect.x(),
					  m_rect.y(),
					  m_rect.width(),
					  15);
	painter->drawText(m_rect, "Constraint");
}

void ConstraintView::setTopLeft(QPointF p)
{
	m_rect = {p.x(), p.y(), m_rect.width(), m_rect.height()};

	m_button->setPos(m_rect.x() + 30, m_rect.y());
}

void ConstraintView::mousePressEvent(QGraphicsSceneMouseEvent* m)
{
    emit constraintPressed(m->pos());
}

void ConstraintView::mouseReleaseEvent(QGraphicsSceneMouseEvent *m)
{
    emit constraintReleased(m->pos());
}
