#include "InspectorPlugin.hpp"
#include "Panel/InspectorPanelFactory.hpp"
using namespace iscore;

InspectorPlugin::InspectorPlugin() :
	QObject {},
	iscore::Autoconnect_QtInterface {},
	iscore::PanelFactoryInterface_QtInterface {}
{
	setObjectName ("InspectorPlugin");
}





QList<Autoconnect> InspectorPlugin::autoconnect_list() const
{
	return
	{
		/// Common
		{{iscore::Autoconnect::ObjectRepresentationType::QObjectName,
		  "Presenter",			 SIGNAL(elementSelected(QObject*))},
		 {iscore::Autoconnect::ObjectRepresentationType::QObjectName,
		  "InspectorPanelModel", SLOT(newItemInspected(QObject*))}},

		{{iscore::Autoconnect::ObjectRepresentationType::Inheritance,
		  "InspectorWidgetBase", SIGNAL(submitCommand(iscore::SerializableCommand*))},
		 {iscore::Autoconnect::ObjectRepresentationType::QObjectName,
		  "Presenter", SLOT(applyCommand(iscore::SerializableCommand*))}},
	};
}



QStringList InspectorPlugin::panel_list() const
{
	return {"Inspector Panel"};
}

PanelFactoryInterface* InspectorPlugin::panel_make (QString name)
{
	if (name == "Inspector Panel")
	{
		return new InspectorPanelFactory;
	}

	return nullptr;
}
