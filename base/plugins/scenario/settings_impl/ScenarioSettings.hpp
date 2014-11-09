#pragma once
#include <interface/settingsdelegate/SettingsDelegateFactoryInterface.hpp>
#include <QObject>


class ScenarioSettings : public iscore::SettingsDelegateFactoryInterface
{
	public:
		ScenarioSettings();
		virtual ~ScenarioSettings() = default;

		// SettingsGroup interface
	public:
		virtual iscore::SettingsDelegateViewInterface* makeView() override;
		virtual iscore::SettingsDelegatePresenterInterface* makePresenter(iscore::SettingsPresenter*,
															  iscore::SettingsDelegateModelInterface* m,
															  iscore::SettingsDelegateViewInterface* v) override;
		virtual iscore::SettingsDelegateModelInterface* makeModel() override;
};

