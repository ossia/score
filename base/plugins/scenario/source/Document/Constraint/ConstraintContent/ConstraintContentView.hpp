#pragma once
#include <QGraphicsObject>

class ConstraintContentView : public QGraphicsObject
{
	Q_OBJECT

	public:
		ConstraintContentView(QGraphicsObject* parent);
		virtual ~ConstraintContentView() = default;


		virtual QRectF boundingRect() const override;
		virtual void paint(QPainter *painter,
						   const QStyleOptionGraphicsItem *option,
						   QWidget *widget) override;


	private:

};
