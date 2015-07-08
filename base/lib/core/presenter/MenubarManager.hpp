#pragma once
#include <QObject>
#include <QAction>
#include <QMenuBar>
#include <map>
#include <core/presenter/Action.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <QDebug>

class QMenuBar;
class QMenu;

namespace iscore
{
    /**
     * @brief The MenubarManager class
     *
     * These are mainly convenience methods to add elements in a non-anarchic way
     * to the menu bar, using the information in \c{MenuInterface}.
     */
    class MenubarManager : public QObject
    {
            Q_OBJECT
        public:
            explicit MenubarManager(QMenuBar* bar, QObject* parent = 0);

            QMenuBar* menuBar() const
            {
                return m_menuBar;
            }

            void insertActionIntoToplevelMenu(ToplevelMenuElement tl, QAction* before, QAction* act);
            void insertActionIntoToplevelMenu(ToplevelMenuElement, QAction* act);

            template<typename MenuElement, typename Functor>
            QAction* addActionIntoToplevelMenu(ToplevelMenuElement tl,
                                               MenuElement elt,
                                               Functor f,
                                               typename std::enable_if<std::is_enum<MenuElement>::value>::type* = 0)
            {
                QAction* act = new QAction {MenuInterface::name(elt), this};
                connect(act, &QAction::triggered, f);
                insertActionIntoToplevelMenu(tl, act);
                return act;
            }

            template<typename MenuElement>
            void insertActionIntoToplevelMenu(ToplevelMenuElement tl,
                                              MenuElement before,
                                              QAction* act,
                                              typename std::enable_if<std::is_enum<MenuElement>::value>::type* = 0)
            {
                auto actions = m_menusMap[tl]->actions();
                auto beforeact_it = std::find_if(
                                        actions.begin(),
                                        actions.end(),
                                        [&before](QAction * theAct)
                {
                    return theAct->objectName() == MenuInterface::name(before);
                });

                if(beforeact_it != actions.end())
                {
                    m_menusMap[tl]->insertAction(*beforeact_it, act);
                }
                else
                {
                    m_menusMap[tl]->addAction(act);
                }
            }

            template<typename MenuElement>
            void addSeparatorIntoToplevelMenu(ToplevelMenuElement tl,
                                              MenuElement sep_type)
            {
                QAction* sep_act = new QAction {this};
                sep_act->setObjectName(MenuInterface::name(sep_type));
                sep_act->setSeparator(true);
                insertActionIntoToplevelMenu(tl,
                                             sep_act);
            }

            template<typename MenuElement>
            void addMenuIntoToplevelMenu(ToplevelMenuElement tl,
                                         MenuElement menu)
            {
                auto act = m_menusMap[tl]->addMenu(MenuInterface::name(menu))->menuAction();
                act->setObjectName(MenuInterface::name(menu));
            }

        signals:

        public slots:
            void insertActionIntoMenubar(PositionedMenuAction);

        private:
            QMenuBar* m_menuBar {};
            std::map<ToplevelMenuElement, QMenu*> m_menusMap;
    };
}
