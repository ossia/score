#include <core/presenter/Presenter.hpp>
#include <interface/customcommand/CustomCommand.hpp>
#include <core/presenter/Command.hpp>
using namespace iscore;

Presenter::Presenter(Model* model, View* view):
	m_model(model),
	m_view(view)
{

}

void Presenter::addCustomCommand(CustomCommand* cmd)
{
	cmd->setParent(this); // Ownership transfer
	cmd->setPresenter(this);
	connect(cmd, &CustomCommand::submitCommand,
			this, &Presenter::applyCommand);

	m_customCommands.push_back(cmd);
}

void Presenter::applyCommand(Command*)
{

}
