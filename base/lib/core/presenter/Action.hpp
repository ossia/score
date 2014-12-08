#pragma once
#include <QString>
class QAction;

namespace iscore
{
	/**
	 * @brief The PositionedMenuAction class
	 *
	 * It is used to specify paths in menus like :
	 * File/MyPlugin/SomeSubmenu/AnotherSubmenu
	 *
	 */
	class PositionedMenuAction
	{
		public:
			// Ex. Fichier/SousMenu
			QString path; // Think of something better.
			QAction* action;
	};
}
