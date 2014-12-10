#include "ScenarioControl.hpp"
#include <Commands/Scenario/CreateEventCommand.hpp>
#include <Commands/Scenario/CreateEventAfterEventCommand.hpp>

ScenarioControl::ScenarioControl(QObject* parent):
	PluginControlInterface{"ScenarioControl", parent}
{

}

void ScenarioControl::populateMenus(iscore::MenubarManager*)
{
}

void ScenarioControl::populateToolbars()
{
}

void ScenarioControl::setPresenter(iscore::Presenter*)
{
}

iscore::SerializableCommand* ScenarioControl::instantiateUndoCommand(QString name, QByteArray data)
{
	// TODO call serialize() in Command(QByteArray&) constructor.
	iscore::SerializableCommand* cmd{};
	if(name == "CreateEventCommand")
	{
		cmd = new CreateEventCommand;
	}
	else if(name == "CreateEventAfterEventCommand")
	{
		cmd = new CreateEventAfterEventCommand;
	}
	else
	{
		qDebug() << Q_FUNC_INFO << "Warning : command received, but it could not be read.";
		return nullptr;
	}

	cmd->deserialize(data);
	return cmd;

}
