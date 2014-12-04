#pragma once
#include <QNamedObject>
#include <QMouseEvent>

class EventView : public QNamedGraphicsObject
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

	signals:
		void eventPressed();
		void eventReleased(QPointF);

	protected:
		virtual void mousePressEvent(QGraphicsSceneMouseEvent* m) override;
		virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* m) override;

};

