#include <NetworkPlugin.hpp>
#include <NetworkCommand.hpp>
//#include <NetworkPanel.hpp>
#include <settings_impl/NetworkSettings.hpp>

#define PROCESS_NAME "Network Process"
#define CMD_NAME "Networkigate"
#define MAIN_PANEL_NAME "NetworkCentralPanel"
#define SECONDARY_PANEL_NAME "NetworkSmallPanel"

NetworkPlugin::NetworkPlugin():
	QObject{},
	iscore::AutoconnectFactoryPluginInterface{},
	iscore::CustomCommandFactoryPluginInterface{},
//	iscore::PanelFactoryPluginInterface{},
	iscore::SettingsFactoryPluginInterface{}
{
	setObjectName("NetworkPlugin");
}

// Interfaces implementations :

//////////////////////////
QList<iscore::Autoconnect> NetworkPlugin::autoconnect_list() const
{
	return {
			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "Document",		SIGNAL(newDocument_start())},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "NetworkCommand", SLOT(setupMasterSession())}},

			// Emission
			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "CommandQueue",	SIGNAL(push_start(iscore::Command*))},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "NetworkCommand", SLOT(commandPush(iscore::Command*))}},
			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "CommandQueue",	SIGNAL(onUndo())},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "NetworkCommand", SLOT(sendUndo())}},
			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "CommandQueue",	SIGNAL(onRedo())},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "NetworkCommand", SLOT(sendRedo())}},


			// Reception
			{{iscore::Autoconnect::ObjectRepresentationType::Inheritance, "RemoteActionReceiver", SIGNAL(receivedCommand(QString, QString, QByteArray))},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "Presenter",			  SLOT(instantiateUndoCommand(QString, QString, QByteArray))}},

			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "Presenter",				SIGNAL(instantiatedCommand(iscore::Command*))},
			 {iscore::Autoconnect::ObjectRepresentationType::Inheritance, "RemoteActionReceiver",	SLOT(applyCommand(iscore::Command*))}}
		   };
}

//////////////////////////
iscore::SettingsGroup* NetworkPlugin::settings_make()
{
	return new NetworkSettings;
}

//////////////////////////
QStringList NetworkPlugin::customCommand_list() const
{
	return {CMD_NAME};
}

iscore::CustomCommand* NetworkPlugin::customCommand_make(QString name)
{
	if(name == QString(CMD_NAME))
	{
		return new NetworkCommand;
	}

	return nullptr;
}

/*
QStringList NetworkPlugin::panel_list() const
{
	return {SECONDARY_PANEL_NAME};
}

iscore::Panel* NetworkPlugin::panel_make(QString name)
{
	if(name == QString(SECONDARY_PANEL_NAME))
	{
		return new NetworkPanel;
	}

	return nullptr;
}
*/
