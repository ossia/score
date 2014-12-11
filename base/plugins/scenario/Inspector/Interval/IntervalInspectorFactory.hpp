#pragma once
#include <QObject>
#include <interface/inspector/InspectorWidgetFactoryInterface.hpp>


class IntervalInspectorFactory : public InspectorWidgetFactoryInterface
{
	public:
		IntervalInspectorFactory() :
			InspectorWidgetFactoryInterface {}
		{

		}

		virtual InspectorWidgetBase* makeWidget (QObject* sourceElement) override;
		virtual InspectorWidgetBase* makeWidget (QList<QObject*> sourceElements) override;

		virtual QString correspondingObjectName() const override
		{
			return "IntervalModel";
		}
};
