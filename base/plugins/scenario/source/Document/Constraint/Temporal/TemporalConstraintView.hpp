#pragma once
#include <QGraphicsObject>
#include <QGraphicsProxyWidget>
#include <QPointF>

class TemporalConstraintView : public QGraphicsObject
{
	Q_OBJECT

	public:
		TemporalConstraintView(QGraphicsObject* parent);

		virtual ~TemporalConstraintView() = default;

		virtual QRectF boundingRect() const override;
		virtual void paint(QPainter *painter,
						   const QStyleOptionGraphicsItem *option,
						   QWidget *widget) override;

		//void setTopLeft(QPointF p);
		void setWidth(int width);
		void setHeight(int height);

		//QRectF m_rect;
		QLineF firstLine;
		QLineF secondLine;

	signals:
		void constraintPressed(QPointF);
		void constraintReleased(QPointF);

	protected:
		virtual void mousePressEvent(QGraphicsSceneMouseEvent* m) override;
		virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* m) override;

	private:
		int m_width{};
		int m_height{};

		QPointF m_clickedPoint{};
};

