#pragma once
#include <interface/customcommand/CustomCommand.hpp>
#include <QAction>
class HelloWorldCommand : public iscore::CustomCommand
{
	public:
		HelloWorldCommand();
		virtual void populateMenus();
		virtual void populateToolbars();
		virtual void setPresenter(iscore::Presenter*);

	private:
		QAction* m_action_HelloWorldigate;
};
