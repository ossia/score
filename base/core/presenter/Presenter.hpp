#pragma once
#include <core/presenter/MenubarManager.hpp>


#include <set>
#include <core/document/Document.hpp>

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
			Document* document() { return m_document; }

			void setupCommand(CustomCommand*);
			void addPanel(Panel*);

		signals:
			void instantiatedCommand(iscore::Command*);

		public slots:
			void newDocument();
			void applyCommand(Command*);
			void instantiateUndoCommand(QString parent_name,
										QString name,
										QByteArray data);

		private:
			void setupMenus();

			Document* m_document{};
			MenubarManager m_menubar;

			Model* m_model;
			View* m_view;

			std::vector<CustomCommand*> m_customCommands;
			std::set<PanelPresenter*> m_panelsPresenters;
	};
}
