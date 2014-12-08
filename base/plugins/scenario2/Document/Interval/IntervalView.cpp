#include "IntervalView.hpp"
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QWidget>
#include <QGraphicsProxyWidget>
#include <QPushButton>
IntervalView::IntervalView(QGraphicsObject* parent):
	QGraphicsObject{parent}
{
	this->setParentItem(parent);

	// TODO hack. How to do it properly ?
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

QRectF IntervalView::boundingRect() const
{
	return m_rect;
}

void IntervalView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	painter->drawRect(m_rect);
	painter->drawRect(m_rect.x(),
					  m_rect.y(),
					  m_rect.width(),
					  15);
	painter->drawText(m_rect, "Constraint");
}

void IntervalView::setTopLeft(QPointF p)
{
	m_rect = {p.x(), p.y(), m_rect.width(), m_rect.height()};

	m_button->setPos(m_rect.x() + 30, m_rect.y());
}

void IntervalView::mousePressEvent(QGraphicsSceneMouseEvent* m)
{
	emit intervalPressed();
}

void IntervalView::mouseReleaseEvent(QGraphicsSceneMouseEvent *m)
{
	emit intervalReleased(QPointF( (m->pos().x() - m_rect.left()), m->pos().y() ) );
}
