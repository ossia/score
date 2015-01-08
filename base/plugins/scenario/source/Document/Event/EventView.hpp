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

		QLineF m_firstLine{-15, -15, 0, 0};
		QLineF m_secondLine{-15, -15, 0, 0};

	public slots:
		void setLinesExtremity(int topPoint, int bottomPoint);

	signals:
		void eventPressed();
        void eventReleasedWithControl(QPointF, QPointF);
		void eventReleased(QPointF);

	protected:
		virtual void mousePressEvent(QGraphicsSceneMouseEvent* m) override;
		virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* m) override;
		virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* m) override;

	private:
		QPointF m_clickedPoint{};
};

