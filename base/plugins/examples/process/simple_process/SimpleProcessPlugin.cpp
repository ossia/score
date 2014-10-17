#include <SimpleProcessPlugin.hpp>
#include <HelloWorldProcess.hpp>
#include <settings_impl/HelloWorldSettings.hpp>

#define PROCESS_NAME "HelloWorld Process"
QList<iscore::Autoconnect> SimpleProcessPlugin::autoconnect_list() const
{
	return {
				{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "HelloWorldSettingsModel", SIGNAL(textChanged())},
				 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "HelloWorldProcessModel", SLOT(setText())}}
		   };
}

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


iscore::SettingsGroup* SimpleProcessPlugin::settings_make()
{
	return new HelloWorldSettings;
}
