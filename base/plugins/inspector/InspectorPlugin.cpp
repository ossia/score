#include "InspectorPlugin.hpp"
#include "Panel/InspectorPanelFactory.hpp"
using namespace iscore;

InspectorPlugin::InspectorPlugin() :
	QObject {},
        iscore::Autoconnect_QtInterface {},
//	iscore::PluginControlInterface_QtInterface{},
iscore::PanelFactoryInterface_QtInterface {}
{
	setObjectName ("InspectorPlugin");
}





QList<Autoconnect> InspectorPlugin::autoconnect_list() const
{
	return
	{
		{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, 
		  "Presenter",			 SIGNAL(elementSelected(QObject*))},
		 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, 
		  "InspectorPanelModel", SLOT(newItemInspected(QObject*))}}
	};
}


/*
QStringList InspectorPlugin::control_list() const
{
	return {""};
}

PluginControlInterface* InspectorPlugin::control_make(QString)
{
	return nullptr;
}*/



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
