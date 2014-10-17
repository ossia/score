#pragma once
#include <interface/settings/SettingsGroup.hpp>
#include <QObject>


class HelloWorldSettings : public iscore::SettingsGroup
{
	public:
		HelloWorldSettings();
		virtual ~HelloWorldSettings() = default;

		// SettingsGroup interface
	public:
		virtual iscore::SettingsGroupView* makeView() override;
		virtual iscore::SettingsGroupPresenter* makePresenter(iscore::SettingsPresenter*, 
															  iscore::SettingsGroupModel* m, 
															  iscore::SettingsGroupView* v) override;
		virtual iscore::SettingsGroupModel* makeModel() override;
};

