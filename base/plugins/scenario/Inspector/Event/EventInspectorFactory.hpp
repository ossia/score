#pragma once
#include <QObject>
#include <interface/inspector/InspectorWidgetFactoryInterface.hpp>


class EventInspectorFactory : public InspectorWidgetFactoryInterface
{
	public:
		EventInspectorFactory() :
			InspectorWidgetFactoryInterface {}
		{

		}

		virtual InspectorWidgetBase* makeWidget (QObject* sourceElement) override;
		virtual InspectorWidgetBase* makeWidget (QList<QObject*> sourceElements) override;

		virtual QString correspondingObjectName() const override
		{
			return "EventModel";
		}
};
