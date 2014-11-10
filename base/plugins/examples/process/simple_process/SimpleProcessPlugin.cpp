#include "SimpleProcessPlugin.hpp"
#include <HelloWorldProcess.hpp>
#include <HelloWorldCommand.hpp>
#include <HelloWorldPanel.hpp>
#include <HelloWorldCentralPanel.hpp>

#define PROCESS_NAME "HelloWorld Process"
#define CMD_NAME "HelloWorldigate"
#define MAIN_PANEL_NAME "HelloWorldCentralPanel"
#define SECONDARY_PANEL_NAME "HelloWorldSmallPanel"

SimpleProcessPlugin::SimpleProcessPlugin():
	QObject{},
	iscore::Autoconnect_QtInterface{},
	iscore::PluginControlInterface_QtInterface{},
	iscore::ProcessFactoryInterface_QtInterface{},
	iscore::PanelFactoryInterface_QtInterface{}
{
	setObjectName("SimpleProcessPlugin");
}

// Interfaces implementations :

//////////////////////////
QList<iscore::Autoconnect> SimpleProcessPlugin::autoconnect_list() const
{
	return {
//			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "HelloWorldSettingsModel", SIGNAL(textChanged())},
//			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "HelloWorldProcessModel", SLOT(setText())}},

			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "HelloWorldCommand", SIGNAL(incrementProcesses())},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "HelloWorldProcessModel", SLOT(increment())}},

			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "HelloWorldCommand", SIGNAL(decrementProcesses())},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "HelloWorldProcessModel", SLOT(decrement())}}
		   };
}

//////////////////////////
QStringList SimpleProcessPlugin::process_list() const
{
	return {PROCESS_NAME};
}

iscore::ProcessFactoryInterface* SimpleProcessPlugin::process_make(QString name)
{
	if(name == QString(PROCESS_NAME))
	{
		return new HelloWorldProcess;
	}

	return nullptr;
}

//////////////////////////
QStringList SimpleProcessPlugin::control_list() const
{
	return {CMD_NAME};
}

iscore::PluginControlInterface* SimpleProcessPlugin::control_make(QString name)
{
	if(name == QString(CMD_NAME))
	{
		return new HelloWorldCommand;
	}

	return nullptr;
}

QStringList SimpleProcessPlugin::panel_list() const
{
	return {/*MAIN_PANEL_NAME, */SECONDARY_PANEL_NAME};
}

iscore::PanelFactoryInterface* SimpleProcessPlugin::panel_make(QString name)
{
	if(name == QString(SECONDARY_PANEL_NAME))
	{
		return new HelloWorldPanel;
	}
	return nullptr;
}


QStringList SimpleProcessPlugin::document_list() const
{
	return {MAIN_PANEL_NAME};
}

iscore::DocumentDelegateFactoryInterface*SimpleProcessPlugin::document_make(QString name)
{
	if(name == QString(MAIN_PANEL_NAME))
	{
		return new HelloWorldCentralPanel;
	}

	return nullptr;
}

