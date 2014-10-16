#pragma once
#include <interface/settings/SettingsGroup.hpp>

class HelloWorldSettings : public iscore::SettingsGroup
{
	public:
		HelloWorldSettings();

		// SettingsGroup interface
	public:
		virtual std::unique_ptr<iscore::SettingsGroupView> makeView() override;
		virtual std::unique_ptr<iscore::SettingsGroupPresenter> makePresenter(iscore::SettingsPresenter*, iscore::SettingsGroupModel* m, iscore::SettingsGroupView* v) override;
		virtual std::unique_ptr<iscore::SettingsGroupModel> makeModel() override;
};

