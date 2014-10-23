#include "HelloWorldCommand.hpp"
#include <core/presenter/Presenter.hpp>
#include <core/presenter/command/Command.hpp>
#include <core/view/View.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <interface/customcommand/MenuInterface.hpp>
#include <QApplication>

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

void HelloWorldCommand::populateMenus(MenubarManager* menu)
{
	// Use identifiers for the common menus
	menu->insertActionIntoMenubar({MenuInterface::name(ToplevelMenuElement::FileMenu) + "/" + tr("trololo"),
								   m_action_HelloWorldigate});
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
	// Exemple of a global command that acts on every object with goes by the name "HelloWorldCommand"
	// We MUST NOT pass pointers of ANY KIND as data because of the distribution needs.
	// If an object must be reference, it has to get an id, a name, or something from which
	// it can be referenced.
	class HelloWorldIncrementCommandImpl : public Command
	{
		public:
			HelloWorldIncrementCommandImpl():
				Command{QString("HelloWorldIncrementCommandImpl"), QString("Increment process")},
				m_parentName{"HelloWorldCommand"}
			{
			}

			virtual QByteArray serialize() override
			{
				auto arr = Command::serialize();
				QDataStream s(&arr, QIODevice::Append);
				s << m_parentName.toLatin1().constData();

				return arr;
			}

			void deserialize(QByteArray arr) override
			{
				QBuffer buf(&arr, nullptr);
				cmd_deserialize(&buf);

				QDataStream stream(&buf);
				char* string;
				stream >> string;
				m_parentName = string;
				delete[] string;
			}

			virtual void undo() override
			{
				auto target = qApp->findChild<HelloWorldCommand*>(m_parentName);
				if(target)
					target->decrementProcesses();
			}

			virtual void redo() override
			{
				auto target = qApp->findChild<HelloWorldCommand*>(m_parentName);
				if(target)
					target->incrementProcesses();
			}

		private:
			QString m_parentName;
	};

	auto cmd = new HelloWorldIncrementCommandImpl();
	emit submitCommand(cmd);
}
