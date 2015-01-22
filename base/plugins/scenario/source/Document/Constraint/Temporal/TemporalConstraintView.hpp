#pragma once
#include <QGraphicsObject>
#include <QGraphicsProxyWidget>
#include <QPointF>
#include <QPen>

class TemporalConstraintViewModel;

class TemporalConstraintView : public QGraphicsObject
{
	Q_OBJECT

	public:
		TemporalConstraintView(TemporalConstraintViewModel* viewModel, QGraphicsObject* parent);

		virtual ~TemporalConstraintView() = default;

		virtual QRectF boundingRect() const override;
		virtual void paint(QPainter *painter,
						   const QStyleOptionGraphicsItem *option,
						   QWidget *widget) override;

		//void setTopLeft(QPointF p);
		void setWidth(int width);
        void setMaxWidth(int max);
        void setMinWidth(int min);
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
        int m_maxWidth{};
        int m_minWidth{};

		int m_height{};

		QPointF m_clickedPoint{};

		TemporalConstraintViewModel* m_viewModel{};

		QPen m_solidPen{
			QBrush{Qt::black},
			4,
			Qt::SolidLine,
			Qt::RoundCap,
			Qt::RoundJoin};
		QPen m_dashPen{
			QBrush{Qt::black},
			4,
			Qt::DashLine,
			Qt::RoundCap,
                    Qt::RoundJoin};
};
