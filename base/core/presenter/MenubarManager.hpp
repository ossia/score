#pragma once
#include <QObject>
#include <map>
#include <core/presenter/Action.hpp>
#include <interface/customcommand/MenuInterface.hpp>

class QMenuBar;
class QMenu;

namespace iscore
{
	class MenubarManager : public QObject
	{
			Q_OBJECT
		public:
			explicit MenubarManager(QMenuBar* bar, QObject *parent = 0);
			

			void insertActionIntoToplevelMenu(ToplevelMenuElement, QAction*);
			
		signals:
			
		public slots:
			void insertActionIntoMenubar(Action);
			
		private:
			QMenuBar* m_menuBar{};
			std::map<ToplevelMenuElement, QMenu*> m_menusMap;
	};
}
