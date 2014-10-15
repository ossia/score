#pragma once
#include <QMainWindow>

namespace iscore
{
	class View
	{
		public:
			View()
			{
				m_mainWindow.show();
			}

		private:
			QMainWindow m_mainWindow;
	};
}
