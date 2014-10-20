#pragma once
#include <QString>
class QAction;

namespace iscore
{
	class Action
	{
		public:
			// Ex. Fichier/SousMenu
			QString path; // Think of something better.
			QAction* action;
	};
}
