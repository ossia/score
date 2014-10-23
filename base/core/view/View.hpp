#pragma once
#include <QMainWindow>
#include <QDebug>
#include <QMenuBar>
#include <set>

#include <core/presenter/Action.hpp>
namespace iscore
{
	class PanelView;
	class Application;
	class View : public QMainWindow
	{
			Q_OBJECT
		public:
			View(QObject* parent);

			void addPanel(PanelView*);
			void setCentralPanel(PanelView*);

		public slots:
			void createZeroconfSelectionDialog();

		signals:
			void insertActionIntoMenubar(Action);

		private:
			std::set<PanelView*> m_panelsViews;
			Application* m_application;
	};
}
