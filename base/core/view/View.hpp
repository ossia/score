#pragma once
#include <QMainWindow>
#include <QDebug>
#include <QMenuBar>
#include <set>

#include <core/presenter/Action.hpp>
namespace iscore
{
	class PanelView;
	class Presenter;
	/**
	 * @brief The View class
	 *
	 * The main display of the application.
	 */
	class View : public QMainWindow
	{
			Q_OBJECT
		public:
			View(QObject* parent);

			void addPanel(PanelView*);
			void setPresenter(Presenter*);

		public slots:

		signals:
			void insertActionIntoMenubar(Action);

		private:
			std::set<PanelView*> m_panelsViews;
			Presenter* m_presenter;
	};
}
