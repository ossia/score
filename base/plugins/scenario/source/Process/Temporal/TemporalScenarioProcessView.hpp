#pragma once
#include "ProcessInterface/ProcessViewInterface.hpp"

class TemporalScenarioProcessView : public ProcessViewInterface
{
	Q_OBJECT

	public:
		TemporalScenarioProcessView(QGraphicsObject* parent);

		virtual ~TemporalScenarioProcessView() = default;

		virtual QRectF boundingRect() const;
		virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

	signals:
		void scenarioPressed();
		void scenarioPressedWithControl(QPointF);
        void scenarioReleased(QPointF, QPointF);

		void deletePressed();

	protected:
		virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
		virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
		virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

		virtual void keyPressEvent(QKeyEvent* event) override;

    private:
        QPointF m_clickedPoint{};
};





