#pragma once
#include <interface/settings/SettingsGroup.hpp>
#include <QObject>


class ScenarioSettings : public iscore::SettingsGroup
{
	public:
		ScenarioSettings();
		virtual ~ScenarioSettings() = default;

		// SettingsGroup interface
	public:
		virtual iscore::SettingsGroupView* makeView() override;
		virtual iscore::SettingsGroupPresenter* makePresenter(iscore::SettingsPresenter*, 
															  iscore::SettingsGroupModel* m, 
															  iscore::SettingsGroupView* v) override;
		virtual iscore::SettingsGroupModel* makeModel() override;
};

