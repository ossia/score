#include "HelloWorldCommand.hpp"
using namespace iscore;
HelloWorldCommand::HelloWorldCommand():
	CustomCommand{},
	m_action_HelloWorldigate{new QAction("HelloWorldigate!", this)}
{

}

void HelloWorldCommand::populateMenus()
{
}

void HelloWorldCommand::populateToolbars()
{
}

void HelloWorldCommand::setPresenter(iscore::Presenter*)
{
}
