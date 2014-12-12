#pragma once
#include <QObject>
#include <InspectorInterface/InspectorWidgetFactoryInterface.hpp>


class ConstraintInspectorFactory : public InspectorWidgetFactoryInterface
{
	public:
		ConstraintInspectorFactory() :
			InspectorWidgetFactoryInterface {}
		{

		}

		virtual InspectorWidgetBase* makeWidget (QObject* sourceElement) override;
		virtual InspectorWidgetBase* makeWidget (QList<QObject*> sourceElements) override;

		virtual QString correspondingObjectName() const override
		{
			return "ConstraintModel";
		}
};
