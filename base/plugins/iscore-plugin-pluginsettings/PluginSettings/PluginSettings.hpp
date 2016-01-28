#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactoryInterface.hpp>

#include <iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegatePresenterInterface.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateViewInterface.hpp>

namespace iscore {
class SettingsPresenter;
}  // namespace iscore

namespace PluginSettings
{
/**
 * @brief The PluginSettings class
 *
 * Base settings for the plug-ins : for now, only a blacklist.
 * Format : save on the config the name of each blacklisted plugin.
 * If a name is not there the plug-in is not blacklisted. Takes effect on next restart ?
 */
class PluginSettingsFactory : public iscore::SettingsDelegateFactoryInterface
{
    public:
        PluginSettingsFactory();
        virtual ~PluginSettingsFactory() = default;

        // SettingsGroup interface
    public:
        iscore::SettingsDelegateViewInterface* makeView() override;
        iscore::SettingsDelegatePresenterInterface* makePresenter(iscore::SettingsPresenter*,
                iscore::SettingsDelegateModelInterface* m,
                iscore::SettingsDelegateViewInterface* v) override;
        iscore::SettingsDelegateModelInterface* makeModel() override;
};

}
