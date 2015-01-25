#pragma once
#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"

class TemporalConstraintViewModel;

class TemporalConstraintView : public AbstractConstraintView
{
	Q_OBJECT

	public:
		TemporalConstraintView(QGraphicsObject* parent);

		virtual ~TemporalConstraintView() = default;

		virtual QRectF boundingRect() const override;
		virtual void paint(QPainter *painter,
						   const QStyleOptionGraphicsItem *option,
						   QWidget *widget) override;

	signals:
		void constraintReleased(QPointF);

	protected:
		virtual void mousePressEvent(QGraphicsSceneMouseEvent* m) override;
		virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* m) override;

	private:
		QPointF m_clickedPoint{};

		TemporalConstraintViewModel* m_viewModel{};
};
