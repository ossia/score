#include "ScenarioCommand.hpp"
#include <core/presenter/Presenter.hpp>
#include <core/presenter/command/Command.hpp>
#include <core/view/View.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <interface/customcommand/MenuInterface.hpp>
#include <QApplication>

using namespace iscore;
ScenarioCommand::ScenarioCommand():
	CustomCommand{},
	m_action_Scenarioigate{new QAction("Scenarioigate!", this)}
{
	this->setObjectName("ScenarioCommand");

	m_action_Scenarioigate->setObjectName("ScenarioigateAction");
	connect(m_action_Scenarioigate, &QAction::triggered,
			this, &ScenarioCommand::on_actionTrigger);
}

void ScenarioCommand::populateMenus(MenubarManager* menu)
{
	// Use identifiers for the common menus
	menu->insertActionIntoMenubar({MenuInterface::name(ToplevelMenuElement::FileMenu) + "/" + tr("trololo"),
								   m_action_Scenarioigate});
}

void ScenarioCommand::populateToolbars()
{
}

void ScenarioCommand::setPresenter(iscore::Presenter* pres)
{
	m_presenter = pres;
}



void ScenarioCommand::on_actionTrigger()
{
	// Exemple of a global command that acts on every object with goes by the name "ScenarioCommand"
	// We MUST NOT pass pointers of ANY KIND as data because of the distribution needs.
	// If an object must be reference, it has to get an id, a name, or something from which
	// it can be referenced.
	class ScenarioIncrementCommandImpl : public Command
	{
		public:
			ScenarioIncrementCommandImpl():
				Command{"ScenarioCommand", "ScenarioIncrementCommandImpl", "Increment process"}
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
				cmd_deserialize(&buf);

				QDataStream stream(&buf);
				int test;
				stream >> test;
			}

			virtual void undo() override
			{
				auto target = qApp->findChild<ScenarioCommand*>(parentName());
				if(target)
					target->decrementProcesses();
			}

			virtual void redo() override
			{
				auto target = qApp->findChild<ScenarioCommand*>(parentName());
				if(target)
					target->incrementProcesses();
			}

		private:
			QString m_parentName;
	};
	auto cmd = new ScenarioIncrementCommandImpl();
	emit submitCommand(cmd);
}
