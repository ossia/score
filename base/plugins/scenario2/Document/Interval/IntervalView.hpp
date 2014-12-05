#pragma once
#include <QNamedObject>
#include <QPointF>

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
        void intervalReleased(QPointF);
		void addScenarioProcessClicked();

	protected:
        virtual void mousePressEvent(QGraphicsSceneMouseEvent* m) override;
        virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* m) override;

	private:
		QGraphicsProxyWidget* m_button{};
};

