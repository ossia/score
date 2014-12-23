#include "ScenarioControl.hpp"

#include "Commands/Scenario/CreateEventCommand.hpp"
#include "Commands/Scenario/CreateEventAfterEventCommand.hpp"
#include "Commands/Scenario/DeleteConstraintCommand.hpp"
#include "Commands/Scenario/DeleteEventCommand.hpp"

#include <interface/plugincontrol/MenuInterface.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <QAction>
ScenarioControl::ScenarioControl(QObject* parent):
	PluginControlInterface{"ScenarioControl", parent},
	m_processList{new ProcessList{this}}
{

}

void ScenarioControl::populateMenus(iscore::MenubarManager* menu)
{
	using namespace iscore;
	QAction* selectAll = new QAction{tr("Select all"), this};
	connect(selectAll,	&QAction::triggered,
			this,		&ScenarioControl::selectAll);

	menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
									   ViewMenuElement::Windows,
									   selectAll);


	QAction* deselectAll = new QAction{tr("Deselect all"), this};
	connect(deselectAll,	&QAction::triggered,
			this,			&ScenarioControl::deselectAll);

	menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
									   ViewMenuElement::Windows,
									   deselectAll);
}

void ScenarioControl::populateToolbars()
{
}

void ScenarioControl::setPresenter(iscore::Presenter*)
{
}

iscore::SerializableCommand* ScenarioControl::instantiateUndoCommand(QString name, QByteArray data)
{
	using namespace Scenario::Command;

	// TODO Continue adding commands here. Maybe use a map ?
	iscore::SerializableCommand* cmd{};
	if(name == "CreateEventCommand")
	{ cmd = new CreateEventCommand;}
	else if(name == "CreateEventAfterEventCommand")
	{cmd = new CreateEventAfterEventCommand;}
	else if(name == "ClearConstraint")
	{cmd = new ClearConstraint;}
	else if(name == "EmptyEventCommand")
	{cmd = new EmptyEventCommand;}
	else
	{
		qDebug() << Q_FUNC_INFO << "Warning : command" << name << "received, but it could not be read.";
		return nullptr;
	}

	cmd->deserialize(data);
	return cmd;

}

#include <QApplication>
#include "Document/BaseElement/BaseElementPresenter.hpp"
void ScenarioControl::selectAll()
{
	auto pres = qApp->findChild<BaseElementPresenter*>("BaseElementPresenter");
	pres->selectAll();
}

void ScenarioControl::deselectAll()
{
	auto pres = qApp->findChild<BaseElementPresenter*>("BaseElementPresenter");
	pres->deselectAll();
}
