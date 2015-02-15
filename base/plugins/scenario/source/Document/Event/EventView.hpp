#pragma once
#include <QGraphicsObject>
#include <QMouseEvent>
#include <QKeyEvent>

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

	signals:
		void eventPressed();
        void eventReleasedWithControl(QPointF, QPointF);
		void eventReleased();
		void eventMoved(QPointF);
		void eventMovedWithControl(QPointF, QPointF);

		// True : ctrl is pressed; false : ctrl is not.
		void ctrlStateChanged(bool);

	protected:
		virtual void mousePressEvent(QGraphicsSceneMouseEvent* m) override;
		virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* m) override;
		virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* m) override;
		virtual void keyPressEvent(QKeyEvent* e) override;
		virtual void keyReleaseEvent(QKeyEvent* e) override;


	private:
		QPointF m_clickedPoint{};
};

