#pragma once
#include <interface/customcommand/CustomCommand.hpp>
#include <QAction>
namespace iscore
{
	class Presenter;
}
class HelloWorldCommand : public iscore::CustomCommand
{
		Q_OBJECT
	public:
		HelloWorldCommand();
		virtual void populateMenus();
		virtual void populateToolbars();
		virtual void setPresenter(iscore::Presenter*);

	signals:
		void incrementProcesses();
		void decrementProcesses();

	private slots:
		void on_actionTrigger();

	private:
		QAction* m_action_HelloWorldigate;
		iscore::Presenter* m_presenter{};
};
