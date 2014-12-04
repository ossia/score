#pragma once
#include <QNamedObject>

class IntervalView : public QNamedGraphicsObject
{
	Q_OBJECT

	public:
		IntervalView(QGraphicsObject* parent);

		virtual ~IntervalView() = default;

		virtual QRectF boundingRect() const override;
		virtual void paint(QPainter *painter,
						   const QStyleOptionGraphicsItem *option,
						   QWidget *widget) override;

		void setTopLeft(QPointF p);

		QRectF m_rect;

	signals:
		void intervalPressed();

	protected:
		virtual void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
};

