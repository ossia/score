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
	iscore::Autoconnect_QtInterface{},
	iscore::PluginControlInterface_QtInterface{},
//	iscore::PanelFactoryPluginInterface{},
	iscore::SettingsDelegateFactoryInterface_QtInterface{}
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
			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "CommandQueue",	SIGNAL(push_start(iscore::SerializableCommand*))},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "NetworkCommand", SLOT(commandPush(iscore::SerializableCommand*))}},
			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "CommandQueue",			SIGNAL(onUndo())},
			 {iscore::Autoconnect::ObjectRepresentationType::Inheritance, "RemoteActionEmitter",	SLOT(undo())}},
			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "CommandQueue",			SIGNAL(onRedo())},
			 {iscore::Autoconnect::ObjectRepresentationType::Inheritance, "RemoteActionEmitter",	SLOT(redo())}},


			// Reception
			{{iscore::Autoconnect::ObjectRepresentationType::Inheritance, "RemoteActionReceiver", SIGNAL(receivedCommand(QString, QString, QByteArray))},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "Presenter",			  SLOT(instantiateUndoCommand(QString, QString, QByteArray))}},

			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "Presenter",				SIGNAL(instantiatedCommand(iscore::SerializableCommand*))},
			 {iscore::Autoconnect::ObjectRepresentationType::Inheritance, "RemoteActionReceiver",	SLOT(applyCommand(iscore::SerializableCommand*))}}
		   };
}

//////////////////////////
iscore::SettingsDelegateFactoryInterface* NetworkPlugin::settings_make()
{
	return new NetworkSettings;
}

//////////////////////////
QStringList NetworkPlugin::control_list() const
{
	return {CMD_NAME};
}

iscore::PluginControlInterface* NetworkPlugin::control_make(QString name)
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
