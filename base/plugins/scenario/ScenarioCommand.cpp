#include "ScenarioCommand.hpp"
#include "ScenarioCommandImpl.hpp"
#include <core/presenter/Presenter.hpp>
#include <core/presenter/command/Command.hpp>
#include <core/view/View.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <interface/plugincontrol/MenuInterface.hpp>
#include <QApplication>

using namespace iscore;
ScenarioCommand::ScenarioCommand():
    PluginControlInterface{}
{
	this->setObjectName("ScenarioCommand");
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
    qDebug() << "receive signal" ;
    auto cmd = new ScenarioCommandImpl(position);
	emit submitCommand(cmd);
}
