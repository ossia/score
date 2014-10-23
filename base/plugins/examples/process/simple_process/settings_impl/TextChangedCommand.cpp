#include "TextChangedCommand.hpp"
#include "HelloWorldSettingsPresenter.hpp"

using namespace iscore;
TextChangedCommand::TextChangedCommand(QString old_text, QString new_text, iscore::SettingsGroupPresenter* pres):
	Command{"TextChangedCommand", "Text change"},
	m_old{old_text},
	m_new{new_text},
	m_presenter{static_cast<HelloWorldSettingsPresenter*>(pres)}
{

}

void TextChangedCommand::undo()
{
	m_presenter->setText(m_old);
}

void TextChangedCommand::redo()
{
	m_presenter->setText(m_new);
}
