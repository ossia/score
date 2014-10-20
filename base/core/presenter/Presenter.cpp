#include <core/presenter/Presenter.hpp>
#include <interface/customcommand/CustomCommand.hpp>
#include <core/presenter/Command.hpp>
#include <core/view/View.hpp>
#include <core/application/Application.hpp>
#include <functional>
using namespace iscore;

Presenter::Presenter(Model* model, View* view, QObject* arg_parent):
	QObject{arg_parent},
	m_model{model},
	m_view{view},
	m_menubar{view->menuBar(), this}
{
	m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu, 
										   m_commandQueue.createUndoAction(this));
	m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu, 
										   m_commandQueue.createRedoAction(this));
	
	auto settings_act = new QAction(tr("Settings"), nullptr);
	
	connect(settings_act, &QAction::triggered,
			qobject_cast<Application*>(parent())->settings()->view(), &SettingsView::exec);
	m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::SettingsMenu,
										   settings_act);
}

void Presenter::addCustomCommand(CustomCommand* cmd)
{
	cmd->setParent(this); // Ownership transfer
	cmd->setPresenter(this);
	connect(cmd, &CustomCommand::submitCommand,
			this, &Presenter::applyCommand);

	cmd->populateMenus(&m_menubar);
	cmd->populateToolbars();

	m_customCommands.push_back(cmd);
}

void Presenter::applyCommand(Command* cmd)
{
	m_commandQueue.push(cmd);
}
