#pragma once
#include <core/presenter/CommandQueue.hpp>

namespace iscore
{
	class Command;
	class CustomCommand;
	class Model;
	class View;
	class Action
	{
		public:
			// Ex. Fichier/SousMenu
			QString path; // Think of something better.
			QAction* action;
	};

	class Presenter : public QObject
	{
			Q_OBJECT
		public:
			Presenter(iscore::Model* model, iscore::View* view, QObject* parent);

			View* view() { return m_view; }
			void addCustomCommand(CustomCommand*);
			void insertActionIntoMenu(Action);

		private slots:
			void applyCommand(Command*);

		private:
			CommandQueue m_commandQueue;
			Model* m_model;
			View* m_view;

			std::vector<CustomCommand*> m_customCommands;
	};
}
