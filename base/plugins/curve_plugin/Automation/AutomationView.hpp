#pragma once

#include <ProcessInterface/ProcessViewInterface.hpp>

class AutomationView : public ProcessViewInterface
{

	public:
		AutomationView(QGraphicsObject* parent);
		virtual QRectF boundingRect() const;
		virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);
};
