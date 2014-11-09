#include "HelloWorldCommand.hpp"
#include <core/presenter/Presenter.hpp>
#include <core/presenter/command/Command.hpp>
#include <core/view/View.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <interface/plugincontrol/MenuInterface.hpp>
#include <QApplication>


using namespace iscore;

// Exemple of a global command that acts on every object with goes by the name "HelloWorldCommand"
// We MUST NOT pass pointers of ANY KIND as data because of the distribution needs.
// If an object must be reference, it has to get an id, a name, or something from which
// it can be referenced.
class HelloWorldIncrementCommandImpl : public Command
{
	public:
		HelloWorldIncrementCommandImpl():
			Command{QString("HelloWorldCommand"),
					QString("HelloWorldIncrementCommandImpl"),
					QString("Increment process")}
		{
		}

		virtual QByteArray serialize() override
		{
			auto arr = Command::serialize();
			{
				QDataStream s(&arr, QIODevice::Append);
				s.setVersion(QDataStream::Qt_5_3);

				s << 42;
			}

			return arr;
		}

		void deserialize(QByteArray arr) override
		{
			QBuffer buf(&arr, nullptr);
			buf.open(QIODevice::ReadOnly);
			cmd_deserialize(&buf);

			QDataStream stream(&buf);
			int test;
			stream >> test;
		}

		virtual void undo() override
		{
			qDebug(Q_FUNC_INFO);
			auto target = qApp->findChild<HelloWorldCommand*>(parentName());
			if(target)
				target->decrementProcesses();
		}

		virtual void redo() override
		{
			qDebug(Q_FUNC_INFO);
			auto target = qApp->findChild<HelloWorldCommand*>(parentName());
			if(target)
				target->incrementProcesses();
		}

	private:
		QString m_parentName;
};


HelloWorldCommand::HelloWorldCommand():
	PluginControlInterface{},
	m_action_HelloWorldigate{new QAction("Action test!", this)}
{
	this->setObjectName("HelloWorldCommand");

	m_action_HelloWorldigate->setObjectName("HelloWorldigateAction");
	connect(m_action_HelloWorldigate, &QAction::triggered,
			this, &HelloWorldCommand::on_actionTrigger);
}

void HelloWorldCommand::populateMenus(MenubarManager* menu)
{
	// Use identifiers for the common menus
	menu->insertActionIntoMenubar({MenuInterface::name(ToplevelMenuElement::FileMenu) + "/" + tr("Test"),
								   m_action_HelloWorldigate});
}

void HelloWorldCommand::populateToolbars()
{
}

void HelloWorldCommand::setPresenter(iscore::Presenter* pres)
{
	m_presenter = pres;
}

Command*HelloWorldCommand::instantiateUndoCommand(QString name, QByteArray data)
{
	qDebug(Q_FUNC_INFO);
	if(name == "HelloWorldIncrementCommandImpl")
	{
		auto cmd = new HelloWorldIncrementCommandImpl;
		cmd->deserialize(data);
		return cmd;
	}

	return nullptr;
}



void HelloWorldCommand::on_actionTrigger()
{
	auto cmd = new HelloWorldIncrementCommandImpl();
	emit submitCommand(cmd);
}
