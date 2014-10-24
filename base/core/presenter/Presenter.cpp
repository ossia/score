#include <interface/customcommand/CustomCommand.hpp>
#include <core/presenter/command/Command.hpp>

#include <core/application/Application.hpp>
#include <core/view/View.hpp>
#include <core/model/Model.hpp>

#include <interface/panels/Panel.hpp>
#include <interface/panels/PanelModel.hpp>
#include <interface/panels/BasePanel.hpp>

#include <core/document/DocumentPresenter.hpp>

#include <functional>

using namespace iscore;

Presenter::Presenter(Model* model, View* view, QObject* arg_parent):
	QObject{arg_parent},
	m_model{model},
	m_view{view},
	m_menubar{view->menuBar(), this}
{
	m_view->setPresenter(this);
	setupMenus();

	connect(m_view,		&View::insertActionIntoMenubar,
			&m_menubar, &MenubarManager::insertActionIntoMenubar);
}

void Presenter::setupCommand(CustomCommand* cmd)
{
//	cmd->setParent(this); // Ownership transfer
	cmd->setPresenter(this);
	connect(cmd,  &CustomCommand::submitCommand,
			this, &Presenter::applyCommand);

	cmd->populateMenus(&m_menubar);
	cmd->populateToolbars();

//	m_customCommands.push_back(cmd);
}

void Presenter::addPanel(Panel* p)
{
	auto model = p->makeModel();
	auto view = p->makeView();
	auto pres = p->makePresenter(this, model, view);

	connect(pres, &PanelPresenter::submitCommand,
			this, &Presenter::applyCommand);

	view->setPresenter(pres);
	model->setPresenter(pres);

	if(dynamic_cast<BasePanel*>(p))
	{
		m_view->setCentralPanel(view);
	}
	else
	{
		m_view->addPanel(view);
	}
	m_model->addPanel(model);
	m_panelsPresenters.insert(pres);
}

void Presenter::newDocument()
{
	Document* doc = new Document(this);
	// Undo / redo
	m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
										   doc->presenter()->commandQueue()->createUndoAction(this));
	m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
										   doc->presenter()->commandQueue()->createRedoAction(this));
}

void Presenter::applyCommand(Command*)
{
	// Appeler le présenteur du doc courant.
}

void Presenter::setupMenus()
{
	// Networking
	QAction* newSession = new QAction{tr("New"), this};
	connect(newSession,									&QAction::triggered,
			qobject_cast<Application*>(parent()),		&Application::on_New);
	m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
										   newSession);

	QAction* joinSession = new QAction{tr("Join"), this};
	connect(joinSession, &QAction::triggered,
			m_view, &View::createZeroconfSelectionDialog);
	m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
										   joinSession);



	auto settings_act = new QAction(tr("Settings"), nullptr);

	connect(settings_act, &QAction::triggered,
			qobject_cast<Application*>(parent())->settings()->view(), &SettingsView::exec);
	m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::SettingsMenu,
										   settings_act);
}


