#pragma once
#include <QString>
class QAction;

namespace iscore
{
	/**
	 * @brief The Action class
	 *
	 * It is used to specify paths in menus like : 
	 * File/MyPlugin/SomeSubmenu/AnotherSubmenu
	 * 
	 * NOTE : very different from QAction, maybe find a better name ?
	 */
	class Action
	{
		public:
			// Ex. Fichier/SousMenu
			QString path; // Think of something better.
			QAction* action;
	};
}
