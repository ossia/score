#pragma once
#include <core/presenter/CommandQueue.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <set>

namespace iscore
{
	class Command;
	class CustomCommand;
	class Model;
	class View;
	class Panel;
	class PanelPresenter;
	class Presenter : public QObject
	{
			Q_OBJECT
		public:
			Presenter(iscore::Model* model, iscore::View* view, QObject* parent);
			
			View* view() { return m_view; }
			void addCustomCommand(CustomCommand*);
			void addPanel(Panel*);

		private slots:
			void applyCommand(Command*);

		private:
			void setupMenus();
			
			CommandQueue m_commandQueue;
			Model* m_model;
			View* m_view;

			std::vector<CustomCommand*> m_customCommands;
			std::set<PanelPresenter*> m_panelsPresenters;
			MenubarManager m_menubar;
	};
}
