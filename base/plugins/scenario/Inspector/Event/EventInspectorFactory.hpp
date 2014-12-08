#pragma once
#include <QObject>
#include <interface/inspector/InspectorWidgetFactoryInterface.hpp>


class EventInspectorFactory : public iscore::InspectorWidgetFactoryInterface
{
	public:
		EventInspectorFactory() :
			iscore::InspectorWidgetFactoryInterface {}
		{

		}
		
		virtual InspectorWidgetBase* makeWidget (QObject* sourceElement) override;
		virtual InspectorWidgetBase* makeWidget (QList<QObject*> sourceElements) override;
		
		virtual QString correspondingObjectName() const override
		{
			return "EventModel";
		}
};
