#pragma once
#include <QString>
#include <interface/inspector/InspectorWidgetFactoryInterface.hpp>

namespace iscore
{
	class InspectorWidgetFactoryInterface_QtInterface
	{
		public:
			virtual ~InspectorWidgetFactoryInterface_QtInterface() = default;

			// One factory per element to inspect. Example : IntervalInspectorFactory, EventInspectorFactory...
			virtual QStringList inspectorFactory_list() const = 0;
			virtual InspectorWidgetFactoryInterface* inspectorFactory_make(QString) = 0;
	};
}

#define InspectorWidgetFactoryInterface_QtInterface_iid "org.ossia.i-score.plugins.InspectorWidgetFactoryInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::InspectorWidgetFactoryInterface_QtInterface, InspectorWidgetFactoryInterface_QtInterface_iid)
