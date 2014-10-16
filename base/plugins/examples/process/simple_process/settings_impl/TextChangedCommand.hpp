#pragma once
#include <core/presenter/Command.hpp>
#include <interface/settings/SettingsGroup.hpp>

class HelloWorldSettingsPresenter;
class TextChangedCommand : public iscore::Command
{
	public:
		TextChangedCommand(QString old_text,
						   QString new_text,
						   iscore::SettingsGroupPresenter* pres);

		virtual void undo() override;
		virtual void redo() override;

	private:
		QString m_old;
		QString m_new;
		HelloWorldSettingsPresenter* m_presenter;
};
