#include "MenubarManager.hpp"
#include <functional>

using namespace iscore;

MenubarManager::MenubarManager(QMenuBar* bar, QObject* parent) :
    QObject(parent),
    m_menuBar {bar}
{
    for(auto& elt : MenuInterface::map<ToplevelMenuElement>())
    {
        m_menusMap[elt.first] = m_menuBar->addMenu(elt.second);
    }
}



void MenubarManager::insertActionIntoMenubar(PositionedMenuAction actionToInsert)
{
    std::function<void (QMenu*, QStringList)> recurse =
        [&](QMenu * menu, QStringList path_lst) -> void
    {
        if(path_lst.empty())   // End recursion
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
            [&car](QAction * act)
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
        auto menu = m_menuBar;
        auto menu_actions = menu->actions();
        auto act_it = std::find_if(menu_actions.begin(),
                                   menu_actions.end(),
                                   [&car](QAction * act)
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
        m_menuBar->insertAction(nullptr, actionToInsert.action);
    }
}

void MenubarManager::insertActionIntoToplevelMenu(ToplevelMenuElement tl, QAction* act)
{
    insertActionIntoToplevelMenu(tl, nullptr, act);
}

void MenubarManager::insertActionIntoToplevelMenu(ToplevelMenuElement tl, QAction* before, QAction* act)
{
    m_menusMap[tl]->insertAction(before, act);
}


