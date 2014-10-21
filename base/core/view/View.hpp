#pragma once
#include <QMainWindow>
#include <QDebug>
#include <QMenuBar>
#include <set>
namespace iscore
{
	class PanelView;
	class View : public QMainWindow
	{
		public:
			View(QObject* parent):
				QMainWindow{}
			{
			}
			
		private:
			std::set<PanelView*> m_panelsViews;
	};
}
