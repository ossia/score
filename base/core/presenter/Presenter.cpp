#include <core/presenter/Presenter.hpp>
#include <interface/customcommand/CustomCommand.hpp>
#include <core/presenter/Command.hpp>
#include <core/view/View.hpp>
#include <functional>
using namespace iscore;

Presenter::Presenter(Model* model, View* view, QObject* parent):
	QObject{parent},
	m_model{model},
	m_view{view}
{
	auto menu_edition = m_view->menuBar()->addMenu("Édition");
	menu_edition->insertAction(0, m_commandQueue.createUndoAction(this));
	menu_edition->insertAction(0, m_commandQueue.createRedoAction(this));
}

void Presenter::addCustomCommand(CustomCommand* cmd)
{
	cmd->setParent(this); // Ownership transfer
	cmd->setPresenter(this);
	connect(cmd, &CustomCommand::submitCommand,
			this, &Presenter::applyCommand);

	cmd->populateMenus();
	cmd->populateToolbars();

	m_customCommands.push_back(cmd);
}

//TODO what happens if two plug-ins add the same action name ?
void Presenter::insertActionIntoMenu(Action actionToInsert)
{
	std::function<void(QMenu*, QStringList)> recurse =
			[&] (QMenu* menu, QStringList path_lst) -> void
	{
		if(path_lst.empty()) // End recursion
		{
			menu->insertAction(0, actionToInsert.action);
		}
		else
		{
			auto car = path_lst.front();
			path_lst.pop_front();

			auto menu_actions = menu->actions();
			auto act_it = std::find_if(menu_actions.begin(),
									   menu_actions.end(),
			[&car] (QAction* act)
			{
						  return act->text() == car;
			});

			// A submenu of a part of the name already exists.
			if(act_it != menu_actions.end())
			{
				QAction* act = *act_it;
				recurse(act->menu(), path_lst);
			}
			else
			{
				auto submenu = menu->addMenu(car);
				recurse(submenu, path_lst);
			}
		}
	};

	// Damned duplication because QMenuBar is not a QMenu...
	QStringList base_path_lst = actionToInsert.path.split('/');
	if(base_path_lst.size() > 0)
	{
		// We have to find the first submenu...

		auto car = base_path_lst.front();
		base_path_lst.pop_front();
		auto menu = m_view->menuBar();
		auto menu_actions = menu->actions();
		auto act_it = std::find_if(menu_actions.begin(),
								   menu_actions.end(),
								   [&car] (QAction* act)
		{
					  return act->text() == car;
		});

		// A submenu of a part of the name already exists.
		if(act_it != menu->actions().end())
		{
			QAction* act = *act_it;
			recurse(act->menu(), base_path_lst);
		}
		else
		{
			auto submenu = menu->addMenu(car);
			recurse(submenu, base_path_lst);
		}
	}
	else
	{
		m_view->menuBar()->insertAction(nullptr, actionToInsert.action);
	}
}

void Presenter::applyCommand(Command* cmd)
{
	m_commandQueue.push(cmd);
}
