#pragma once
#include <interface/process/ProcessViewInterface.hpp>

class ScenarioProcessView : public ProcessViewInterface
{
	Q_OBJECT

	public:
		ScenarioProcessView(QGraphicsObject* parent);

		virtual ~ScenarioProcessView() = default;

		virtual QRectF boundingRect() const;
		virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);


	signals:
		void scenarioPressed(QPointF);
		void scenarioReleased(QPointF);

	protected:
		virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
		virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
		virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};





