#pragma once
#include <interface/settingsdelegate/SettingsDelegateFactoryInterface.hpp>
#include <QObject>


/**
 * @brief The PluginSettings class
 *
 * Base settings for the plug-ins : for now, only a blacklist.
 * Format : save on the config the name of each blacklisted plugin.
 * If a name is not there the plug-in is not blacklisted. Takes effect on next restart ?
 */
class PluginSettings : public iscore::SettingsDelegateFactoryInterface
{
    public:
        PluginSettings();
        virtual ~PluginSettings() = default;

        // SettingsGroup interface
    public:
        virtual iscore::SettingsDelegateViewInterface* makeView() override;
        virtual iscore::SettingsDelegatePresenterInterface* makePresenter(iscore::SettingsPresenter*,
                iscore::SettingsDelegateModelInterface* m,
                iscore::SettingsDelegateViewInterface* v) override;
        virtual iscore::SettingsDelegateModelInterface* makeModel() override;
};

