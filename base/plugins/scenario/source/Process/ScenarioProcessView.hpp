#pragma once
#include "ProcessInterface/ProcessViewInterface.hpp"

class ScenarioProcessView : public ProcessViewInterface
{
	Q_OBJECT

	public:
		ScenarioProcessView(QGraphicsObject* parent);

		virtual ~ScenarioProcessView() = default;

		virtual QRectF boundingRect() const;
		virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

	signals:
		void scenarioPressed();
		void scenarioPressedWithControl(QPointF);
		void scenarioReleased(QPointF);

		void deletePressed();

	protected:
		virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
		virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
		virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

		virtual void keyPressEvent(QKeyEvent* event) override;
};





