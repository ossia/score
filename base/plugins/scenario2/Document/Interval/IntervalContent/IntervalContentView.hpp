#pragma once
#include <QGraphicsObject>

class IntervalContentView : public QGraphicsObject
{
	Q_OBJECT

	public:
		IntervalContentView(QGraphicsObject* parent);
		virtual ~IntervalContentView() = default;


		virtual QRectF boundingRect() const override;
		virtual void paint(QPainter *painter,
						   const QStyleOptionGraphicsItem *option,
						   QWidget *widget) override;


	private:

};
