#pragma once
#include <QObject>
#include <interface/plugins/SettingsDelegateFactoryInterface_QtInterface.hpp>

class PluginSettingsPlugin :
    public QObject,
    public iscore::SettingsDelegateFactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID SettingsDelegateFactoryInterface_QtInterface_iid)
        Q_INTERFACES(iscore::SettingsDelegateFactoryInterface_QtInterface)

    public:
        PluginSettingsPlugin();
        virtual ~PluginSettingsPlugin() = default;

        // Settings interface
        virtual iscore::SettingsDelegateFactoryInterface* settings_make() override;
};
