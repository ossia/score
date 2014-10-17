#include "HelloWorldCommand.hpp"
#include <core/presenter/Presenter.hpp>
#include <core/presenter/Command.hpp>
#include <core/view/View.hpp>

using namespace iscore;
HelloWorldCommand::HelloWorldCommand():
	CustomCommand{},
	m_action_HelloWorldigate{new QAction("HelloWorldigate!", this)}
{
	this->setObjectName("HelloWorldCommand");

	m_action_HelloWorldigate->setObjectName("HelloWorldigateAction");
	connect(m_action_HelloWorldigate, &QAction::triggered,
			this, &HelloWorldCommand::on_actionTrigger);
}

void HelloWorldCommand::populateMenus()
{
	m_presenter->insertActionIntoMenu({"Fichier/trololo", m_action_HelloWorldigate});
}

void HelloWorldCommand::populateToolbars()
{
}

void HelloWorldCommand::setPresenter(iscore::Presenter* pres)
{
	m_presenter = pres;
}



void HelloWorldCommand::on_actionTrigger()
{
	class HelloWorldIncrementCommandImpl : public Command
	{
		public:
			HelloWorldIncrementCommandImpl(HelloWorldCommand* parent):
				Command{"Increment process"},
				m_parent{parent}
			{
			}

			virtual void undo() override
			{
				emit m_parent->decrementProcesses();
			}
			virtual void redo() override
			{
				emit m_parent->incrementProcesses();
			}

		private:
			HelloWorldCommand* m_parent;
	};

	emit submitCommand(new HelloWorldIncrementCommandImpl(this));
}
