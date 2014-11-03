#pragma once
#include <interface/settings/SettingsGroup.hpp>
#include <QObject>


/**
 * @brief The PluginSettings class
 *
 * Base settings for the plug-ins : for now, only a blacklist.
 * Format : save on the config the name of each blacklisted plugin.
 * If a name is not there the plug-in is not blacklisted. Takes effect on next restart ?
 */
class PluginSettings : public iscore::SettingsGroup
{
	public:
		PluginSettings();
		virtual ~PluginSettings() = default;

		// SettingsGroup interface
	public:
		virtual iscore::SettingsGroupView* makeView() override;
		virtual iscore::SettingsGroupPresenter* makePresenter(iscore::SettingsPresenter*,
															  iscore::SettingsGroupModel* m,
															  iscore::SettingsGroupView* v) override;
		virtual iscore::SettingsGroupModel* makeModel() override;
};

