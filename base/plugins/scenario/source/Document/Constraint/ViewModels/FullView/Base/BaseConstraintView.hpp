#pragma once
#include <QGraphicsObject>
#include <QGraphicsProxyWidget>
#include <QPointF>
#include <QPen>

class BaseConstraintViewModel;

class BaseConstraintView : public QGraphicsObject
{
	Q_OBJECT

	public:
		BaseConstraintView(BaseConstraintViewModel* viewModel, QGraphicsObject* parent);

		virtual ~BaseConstraintView() = default;

		virtual QRectF boundingRect() const override;
		virtual void paint(QPainter *painter,
						   const QStyleOptionGraphicsItem *option,
						   QWidget *widget) override;
		void setHeight(int height);	
		void setWidth(int width);

	signals:
		void constraintPressed(QPointF);

	protected:
		virtual void mousePressEvent(QGraphicsSceneMouseEvent* m) override;

	private:
		int m_width{};
		int m_height{};

		BaseConstraintViewModel* m_viewModel{};

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
