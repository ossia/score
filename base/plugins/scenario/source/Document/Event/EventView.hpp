#pragma once
#include <QGraphicsObject>
#include <QMouseEvent>

class EventView : public QGraphicsObject
{
	Q_OBJECT

	public:
		EventView(QGraphicsObject* parent);
		virtual ~EventView() = default;

		virtual QRectF boundingRect() const;
		virtual void paint(QPainter* painter,
						   const QStyleOptionGraphicsItem* option,
						   QWidget* widget);

		virtual void setTopLeft(QPointF p);


		QRectF m_rect{0, 0, 30, 30};

		QLineF m_firstLine{0, 0, 0, 0};
		QLineF m_secondLine{0, 0, 0, 0 };

public slots:
		void setLinesExtremity(int topPoint, int bottomPoint);

	signals:
		void eventPressed();
		void eventReleasedWithControl(QPointF);
		void eventReleased(QPointF);

	protected:
		virtual void mousePressEvent(QGraphicsSceneMouseEvent* m) override;
		virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* m) override;
		virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* m) override;

};

