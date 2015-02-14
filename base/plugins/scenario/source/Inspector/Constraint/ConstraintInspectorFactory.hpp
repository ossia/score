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

		virtual QList<QString> correspondingObjectsNames() const override
		{
			return {"TemporalConstraintViewModel", "FullViewConstraintViewModel"};
		}
};
