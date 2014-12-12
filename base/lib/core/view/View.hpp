#pragma once
#include <QMainWindow>
#include <QDebug>
#include <QMenuBar>
#include <set>

#include <core/presenter/Action.hpp>
namespace iscore
{
	class PanelViewInterface;
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

			void addPanel(PanelViewInterface*);
			void setPresenter(Presenter*);

		public slots:

		signals:
			/**
			 * @brief insertActionIntoMenubar
			 *
			 * A quick signal to add an action.
			 * Especially considering that we already know the presenter.
			 */
			void insertActionIntoMenubar(PositionedMenuAction);

		private:
			std::set<PanelViewInterface*> m_panelsViews;
			Presenter* m_presenter{}; //@todo remove.
	};
}
