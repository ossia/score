#include <SimpleProcessPlugin.hpp>
#include <HelloWorldProcess.hpp>
#include <HelloWorldCommand.hpp>
#include <settings_impl/HelloWorldSettings.hpp>

#define PROCESS_NAME "HelloWorld Process"
#define CMD1_NAME "HelloWorldigate"

SimpleProcessPlugin::SimpleProcessPlugin():
	QObject{},
	iscore::AutoconnectFactoryPluginInterface{},
	iscore::CustomCommandFactoryPluginInterface{},
	iscore::ProcessFactoryPluginInterface{},
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
			{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "HelloWorldProcessModel", SLOT(setText())}}
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
	return {CMD1_NAME};
}

iscore::CustomCommand* SimpleProcessPlugin::customCommand_make(QString name)
{
	if(name == QString(CMD1_NAME))
	{
		return new HelloWorldCommand;
	}
}
