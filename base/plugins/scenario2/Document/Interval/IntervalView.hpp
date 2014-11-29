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

		QRectF m_rect;
};

