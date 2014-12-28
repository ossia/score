#include "ScenarioControl.hpp"

#include "Commands/Scenario/CreateEvent.hpp"
#include "Commands/Scenario/CreateEventAfterEvent.hpp"
#include "Commands/Scenario/ClearConstraint.hpp"
#include "Commands/Scenario/ClearEvent.hpp"

#include <interface/plugincontrol/MenuInterface.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <QAction>
#include <QJsonDocument>

#include <QApplication>

#include <QFileDialog>

#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
ScenarioControl::ScenarioControl(QObject* parent):
	PluginControlInterface{"ScenarioControl", parent},
	m_processList{new ProcessList{this}}
{

}

void ScenarioControl::populateMenus(iscore::MenubarManager* menu)
{
	using namespace iscore;
	// Load & save
	menu->addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
									FileMenuElement::Save,
									[] ()
	{
		auto model = qApp->findChild<BaseElementModel*>("BaseElementModel");
		QJsonDocument doc;

		Serializer<JSON> s;
		s.readFrom(*model->constraintModel());

		doc.setObject(s.m_obj);
		auto savename = QFileDialog::getSaveFileName(nullptr, tr("Save"));

		qDebug() << QString(doc.toJson());
		QFile f(savename);
		f.open(QIODevice::WriteOnly);
		f.write(doc.toJson());
	});

	// View
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
	{ cmd = new CreateEvent;}
	else if(name == "CreateEventAfterEventCommand")
	{cmd = new CreateEventAfterEvent;}
	else if(name == "ClearConstraint")
	{cmd = new ClearConstraint;}
	else if(name == "EmptyEventCommand")
	{cmd = new ClearEvent;}
	else
	{
		qDebug() << Q_FUNC_INFO << "Warning : command" << name << "received, but it could not be read.";
		return nullptr;
	}

	cmd->deserialize(data);
	return cmd;

}

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
