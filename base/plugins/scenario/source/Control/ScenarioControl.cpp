#include "ScenarioControl.hpp"

#include "Commands/Constraint/Box/Deck/AddProcessViewModelToDeck.hpp"
#include "Commands/Constraint/Box/Deck/RemoveProcessViewModelFromDeck.hpp"
#include "Commands/Constraint/Box/Deck/ResizeDeckVertically.hpp"
#include "Commands/Constraint/Box/AddDeckToBox.hpp"
#include "Commands/Constraint/Box/RemoveDeckFromBox.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Constraint/RemoveBoxFromConstraint.hpp"
#include "Commands/Constraint/RemoveProcessFromConstraint.hpp"
#include "Commands/Constraint/SetMinDuration.hpp"
#include "Commands/Constraint/SetMaxDuration.hpp"
#include "Commands/Constraint/SetRigidity.hpp"
#include "Commands/Event/AddStateToEvent.hpp"
#include "Commands/Event/SetCondition.hpp"
#include "Commands/Scenario/ClearConstraint.hpp"
#include "Commands/Scenario/ClearEvent.hpp"
#include "Commands/Scenario/RemoveEvent.hpp"
#include "Commands/Scenario/CreateEvent.hpp"
#include "Commands/Scenario/CreateEventAfterEvent.hpp"
#include "Commands/Scenario/HideBoxInViewModel.hpp"
#include "Commands/Scenario/MoveEvent.hpp"
#include "Commands/Scenario/MoveConstraint.hpp"
#include "Commands/Scenario/ShowBoxInViewModel.hpp"
#include "Commands/RemoveMultipleElements.hpp"

#include <interface/plugincontrol/MenuInterface.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <QAction>
#include <QApplication>

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <QJsonDocument>


#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"

#include "Control/OldFormatConversion.hpp"

ScenarioControl::ScenarioControl(QObject* parent):
	PluginControlInterface{"ScenarioControl", parent},
	m_processList{new ProcessList{this}}
{

}

void ScenarioControl::populateMenus(iscore::MenubarManager* menu)
{
	// TODO the stuff here must apply on the current document
	// We have to chase the findchild<DocumentDelegate.../BaseElement...>.
	using namespace iscore;

	// File

	// Export in old format
	auto toZeroTwo = new QAction("To i-score 0.2", this);
	connect(toZeroTwo, &QAction::triggered,
			[this] ()
	{
		auto savename = QFileDialog::getSaveFileName(nullptr, tr("Save"));

		if(!savename.isEmpty())
		{
			auto bem = qApp->findChild<iscore::DocumentDelegateModelInterface*>("BaseElementModel");

			QFile f(savename);
			f.open(QIODevice::WriteOnly);
			f.write(JSONToZeroTwo(bem->toJson()).toLatin1().constData());
		}
	});


	menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
									   FileMenuElement::Separator_Quit,
									   toZeroTwo);


	// Save as json
	// TODO this should go in the global presenter instead.
	auto toJson = new QAction("To JSON", this);
	connect(toJson, &QAction::triggered,
			[this] ()
	{
		auto savename = QFileDialog::getSaveFileName(nullptr, tr("Save"));

		if(!savename.isEmpty())
		{
			QJsonDocument doc;
			auto bem = qApp->findChild<iscore::DocumentDelegateModelInterface*>("BaseElementModel");
			doc.setObject(bem->toJson());

			QFile f(savename);
			f.open(QIODevice::WriteOnly);
			f.write(doc.toJson());
		}
	});
	menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
									   FileMenuElement::Separator_Quit,
									   toJson);


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

	iscore::SerializableCommand* cmd{};
		 if(name == "AddProcessViewModelToDeck")		{ cmd = new AddProcessViewModelToDeck;}
	else if(name == "RemoveProcessViewModelFromDeck")	{ cmd = new RemoveProcessViewModelFromDeck;}
	else if(name == "ResizeDeckVertically")				{ cmd = new ResizeDeckVertically;}
	else if(name == "AddDeckToBox")						{ cmd = new AddDeckToBox;}
	else if(name == "RemoveDeckFromBox")				{ cmd = new RemoveDeckFromBox;}
	else if(name == "AddBoxToConstraint")				{ cmd = new AddBoxToConstraint;}
	else if(name == "AddProcessToConstraint")			{ cmd = new AddProcessToConstraint;}
	else if(name == "RemoveBoxFromConstraint")			{ cmd = new RemoveBoxFromConstraint;}
	else if(name == "RemoveProcessFromConstraint")		{ cmd = new RemoveProcessFromConstraint;}
	else if(name == "AddStateToEvent")					{ cmd = new AddStateToEvent;}
	else if(name == "SetCondition")						{ cmd = new SetCondition;}
	else if(name == "ClearConstraint")					{ cmd = new ClearConstraint;}
	else if(name == "ClearEvent")						{ cmd = new ClearEvent;}
	else if(name == "CreateEvent")						{ cmd = new CreateEvent;}
    else if(name == "RemoveEvent")						{ cmd = new RemoveEvent;}
    else if(name == "CreateEventAfterEvent")			{ cmd = new CreateEventAfterEvent;}
	else if(name == "HideBoxInViewModel")				{ cmd = new HideBoxInViewModel;}
	else if(name == "MoveConstraint")					{ cmd = new MoveConstraint;}
	else if(name == "MoveEvent")						{ cmd = new MoveEvent;}
	else if(name == "ShowBoxInViewModel")				{ cmd = new ShowBoxInViewModel;}
	else if(name == "RemoveMultipleElements")			{ cmd = new RemoveMultipleElements;}
	else if(name == "SetMinDuration")					{ cmd = new SetMinDuration;}
	else if(name == "SetMaxDuration")					{ cmd = new SetMaxDuration;}
	else if(name == "SetRigidity")						{ cmd = new SetRigidity;}

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
