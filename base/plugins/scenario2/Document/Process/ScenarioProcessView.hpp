#pragma once
#include <interface/process/ProcessViewInterface.hpp>

class ScenarioProcessView : public iscore::ProcessViewInterface
{
	Q_OBJECT

	public:
		ScenarioProcessView(QGraphicsObject* parent);

		virtual ~ScenarioProcessView() = default;

	private:


		// QGraphicsItem interface
	public:
		virtual QRectF boundingRect() const;
		virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);
};





