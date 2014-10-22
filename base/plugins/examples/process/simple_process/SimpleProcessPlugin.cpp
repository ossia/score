#include <SimpleProcessPlugin.hpp>
#include <HelloWorldProcess.hpp>
#include <HelloWorldCommand.hpp>
#include <HelloWorldPanel.hpp>
#include <HelloWorldCentralPanel.hpp>
#include <settings_impl/HelloWorldSettings.hpp>

#define PROCESS_NAME "HelloWorld Process"
#define CMD_NAME "HelloWorldigate"
#define MAIN_PANEL_NAME "HelloWorldCentralPanel"
#define SECONDARY_PANEL_NAME "HelloWorldSmallPanel"

SimpleProcessPlugin::SimpleProcessPlugin():
	QObject{},
	iscore::AutoconnectFactoryPluginInterface{},
	iscore::CustomCommandFactoryPluginInterface{},
	iscore::ProcessFactoryPluginInterface{},
	iscore::PanelFactoryPluginInterface{},
	iscore::SettingsFactoryPluginInterface{}
{
	setObjectName("SimpleProcessPlugin");
}

// Interfaces implementations :

//////////////////////////
QList<iscore::Autoconnect> SimpleProcessPlugin::autoconnect_list() const
{
	return {
			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "HelloWorldSettingsModel", SIGNAL(textChanged())},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "HelloWorldProcessModel", SLOT(setText())}},

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

iscore::Process* SimpleProcessPlugin::process_make(QString name)
{
	if(name == QString(PROCESS_NAME))
	{
		return new HelloWorldProcess;
	}

	return nullptr;
}

//////////////////////////
iscore::SettingsGroup* SimpleProcessPlugin::settings_make()
{
	return new HelloWorldSettings;
}

//////////////////////////
QStringList SimpleProcessPlugin::customCommand_list() const
{
	return {CMD_NAME};
}

iscore::CustomCommand* SimpleProcessPlugin::customCommand_make(QString name)
{
	if(name == QString(CMD_NAME))
	{
		return new HelloWorldCommand;
	}
	
	return nullptr;
}

QStringList SimpleProcessPlugin::panel_list() const
{
	return {MAIN_PANEL_NAME, SECONDARY_PANEL_NAME};
}

iscore::Panel* SimpleProcessPlugin::panel_make(QString name)
{
	if(name == QString(MAIN_PANEL_NAME))
	{
		return new HelloWorldCentralPanel;
	}
	else if(name == QString(SECONDARY_PANEL_NAME))
	{
		return new HelloWorldPanel;
	}
	
	return nullptr;
}

