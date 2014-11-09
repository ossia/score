#pragma once
#include <core/presenter/command/Command.hpp>
#include <interface/settingsdelegate/SettingsDelegateFactoryInterface.hpp>

class ScenarioSettingsPresenter;
class TextChangedCommand : public iscore::Command
{
	public:
		TextChangedCommand(QString old_text,
						   QString new_text,
						   iscore::SettingsDelegatePresenterInterface* pres);

		virtual void deserialize(QByteArray) override
		{
		}

		virtual void undo() override;
		virtual void redo() override;

	private:
		QString m_old;
		QString m_new;
		ScenarioSettingsPresenter* m_presenter;

};
