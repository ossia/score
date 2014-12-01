#pragma once
#include <QObject>
#include <QAction>
#include <QMenuBar>
#include <map>
#include <core/presenter/Action.hpp>
#include <interface/plugincontrol/MenuInterface.hpp>

class QMenuBar;
class QMenu;

namespace iscore
{
	/**
	 * @brief The MenubarManager class
	 *
	 * These are mainly convenience methods to add elements in a non-anarchic way
	 * to the menu bar, using the information in \c{MenuInterface}.
	 *
	 * @todo{Test on OS X}
	 */
	class MenubarManager : public QObject
	{
			Q_OBJECT
		public:
			explicit MenubarManager(QMenuBar* bar, QObject *parent = 0);

			void insertActionIntoToplevelMenu(ToplevelMenuElement tl, QAction* before, QAction* act);
			void insertActionIntoToplevelMenu(ToplevelMenuElement, QAction* act);

			template<typename MenuElement, typename Functor>
			void addActionIntoToplevelMenu(ToplevelMenuElement tl,
										   MenuElement elt,
										   Functor f,
										   typename std::enable_if< std::is_enum<MenuElement>::value >::type* = 0)
			{
				QAction* act = new QAction{MenuInterface::name(elt), this};
				connect(act, &QAction::triggered, f);
				insertActionIntoToplevelMenu(tl, act);
			}

			template<typename MenuElement>
			void insertActionIntoToplevelMenu(ToplevelMenuElement tl,
											  MenuElement before,
											  QAction* act,
											  typename std::enable_if< std::is_enum<MenuElement>::value >::type* = 0)
			{
				auto actions = m_menusMap[tl]->actions();
				auto beforeact_it = std::find_if(
										actions.begin(),
										actions.end(),
										[&before] (QAction* act)
										{ return act->objectName() == MenuInterface::name(before); });

				if(beforeact_it != actions.end())
					m_menusMap[tl]->insertAction(*beforeact_it, act);
			}

			template<typename MenuElement>
			void addSeparatorIntoToplevelMenu(ToplevelMenuElement tl,
											  MenuElement sep_type)
			{
				QAction* sep_act = new QAction{this};
				sep_act->setObjectName(MenuInterface::name(sep_type));
				sep_act->setSeparator(true);
				insertActionIntoToplevelMenu(tl,
											 sep_act);
			}

		signals:

		public slots:
			void insertActionIntoMenubar(Action);

		private:
			QMenuBar* m_menuBar{};
			std::map<ToplevelMenuElement, QMenu*> m_menusMap;
	};
}
