#include <SimpleProcessPlugin.hpp>
#include <ScenarioProcess.hpp>
#include <ScenarioCommand.hpp>
#include <ScenarioCentralPanel.hpp>
#include <settings_impl/ScenarioSettings.hpp>

#define PROCESS_NAME "Scenario Process"
#define CMD_NAME "Scenarioigate"
#define MAIN_PANEL_NAME "ScenarioCentralPanel"
#define SECONDARY_PANEL_NAME "ScenarioSmallPanel"

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
			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "ScenarioSettingsModel", SIGNAL(textChanged())},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "ScenarioProcessModel", SLOT(setText())}},

			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "ScenarioCommand", SIGNAL(incrementProcesses())},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "ScenarioProcessModel", SLOT(increment())}},

			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "ScenarioCommand", SIGNAL(decrementProcesses())},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "ScenarioProcessModel", SLOT(decrement())}},

			{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "ScenarioPresenter", SIGNAL(addTimeEvent(QPointF))},
			 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "ScenarioCommand", SLOT(on_createTimeEvent(QPointF))}}
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
		return new ScenarioProcess;
	}

	return nullptr;
}

//////////////////////////
iscore::SettingsGroup* SimpleProcessPlugin::settings_make()
{
	return new ScenarioSettings;
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
		return new ScenarioCommand;
	}

	return nullptr;
}

QStringList SimpleProcessPlugin::document_list() const
{
	return {MAIN_PANEL_NAME};
}

iscore::DocumentPanel*SimpleProcessPlugin::document_make(QString name)
{
	if(name == QString(MAIN_PANEL_NAME))
	{
		return new ScenarioCentralPanel;
	}

	return nullptr;
}


