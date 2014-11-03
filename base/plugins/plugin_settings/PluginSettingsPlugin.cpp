#include <PluginSettingsPlugin.hpp>
#include <settings_impl/PluginSettings.hpp>

#define PROCESS_NAME "Plugin Process"
#define CMD_NAME "Pluginigate"
#define MAIN_PANEL_NAME "PluginCentralPanel"
#define SECONDARY_PANEL_NAME "PluginSmallPanel"

PluginSettingsPlugin::PluginSettingsPlugin():
	QObject{},
	iscore::AutoconnectFactoryPluginInterface{},
	iscore::SettingsFactoryPluginInterface{}
{
	setObjectName("Plugin Settings");
}

// Interfaces implementations :

//////////////////////////
QList<iscore::Autoconnect> PluginSettingsPlugin::autoconnect_list() const
{
	return {
			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "Document",		SIGNAL(newDocument_start())},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "PluginCommand", SLOT(setupMasterSession())}},

			// Emission
			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "CommandQueue",	SIGNAL(push_start(iscore::Command*))},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "PluginCommand", SLOT(commandPush(iscore::Command*))}},
			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "CommandQueue",			SIGNAL(onUndo())},
			 {iscore::Autoconnect::ObjectRepresentationType::Inheritance, "RemoteActionEmitter",	SLOT(undo())}},
			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "CommandQueue",			SIGNAL(onRedo())},
			 {iscore::Autoconnect::ObjectRepresentationType::Inheritance, "RemoteActionEmitter",	SLOT(redo())}},


			// Reception
			{{iscore::Autoconnect::ObjectRepresentationType::Inheritance, "RemoteActionReceiver", SIGNAL(receivedCommand(QString, QString, QByteArray))},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "Presenter",			  SLOT(instantiateUndoCommand(QString, QString, QByteArray))}},

			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "Presenter",				SIGNAL(instantiatedCommand(iscore::Command*))},
			 {iscore::Autoconnect::ObjectRepresentationType::Inheritance, "RemoteActionReceiver",	SLOT(applyCommand(iscore::Command*))}}
		   };
}

//////////////////////////
iscore::SettingsGroup* PluginSettingsPlugin::settings_make()
{
	return new PluginSettings;
}

