#include "ScenarioCommand.hpp"
#include "scenarioview.hpp"
#include "panel_impl/timeEvent/timeevent.hpp"
#include <core/presenter/Presenter.hpp>
#include <core/presenter/command/Command.hpp>
#include <core/view/View.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <interface/customcommand/MenuInterface.hpp>
#include <QApplication>

#include <QDebug>

using namespace iscore;
ScenarioCommand::ScenarioCommand():
    CustomCommand{}
//  ,
 //   m_action_Scenarioigate{new QAction("CreateTimeEvent", this)}
{
	this->setObjectName("ScenarioCommand");

 //   m_action_Scenarioigate->setObjectName("CreateTimeEventAction");
}

void ScenarioCommand::populateMenus(MenubarManager* menu)
{
}

void ScenarioCommand::populateToolbars()
{
}

void ScenarioCommand::setPresenter(iscore::Presenter* pres)
{
	m_presenter = pres;
}

void ScenarioCommand::emitCreateTimeEvent(QPointF pos)
{
    emit createTimeEvent(pos);
}


void ScenarioCommand::on_createTimeEvent(QPointF position)
{
	// Exemple of a global command that acts on every object with goes by the name "ScenarioCommand"
	// We MUST NOT pass pointers of ANY KIND as data because of the distribution needs.
	// If an object must be reference, it has to get an id, a name, or something from which
	// it can be referenced.
	class ScenarioIncrementCommandImpl : public Command
	{
		public:
            ScenarioIncrementCommandImpl(QPointF pos):
				Command{"Increment process"},
                m_parentName{"ScenarioCommand"},
                _position{pos}
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
				auto target = qApp->findChild<ScenarioCommand*>(m_parentName);
				if(target)
					target->decrementProcesses();
			}

			virtual void redo() override
			{
				auto target = qApp->findChild<ScenarioCommand*>(m_parentName);
                if(target) {
					target->incrementProcesses();
                    target->emitCreateTimeEvent(_position);

                }
			}

		private:
			QString m_parentName;
            QPointF _position;
	};
    qDebug() << "command" << position;
    auto cmd = new ScenarioIncrementCommandImpl(position);
    emit submitCommand(cmd);
}
