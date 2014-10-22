#pragma once
#include <QMainWindow>
#include <QDebug>
#include <QMenuBar>
#include <set>

#include <core/presenter/Action.hpp>

namespace iscore
{
	class PanelView;
	class View : public QMainWindow
	{
			Q_OBJECT
		public:
			View(QObject* parent):
				QMainWindow{}
			{
			}
			
			void addPanel(PanelView*);
			void setCentralPanel(PanelView*);
			
		signals:
			void insertActionIntoMenubar(Action);
			
		private:
			std::set<PanelView*> m_panelsViews;
	};
}
