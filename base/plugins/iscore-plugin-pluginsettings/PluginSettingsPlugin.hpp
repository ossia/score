#pragma once
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/SettingsDelegateFactoryInterface_QtInterface.hpp>
#include <QObject>

#include <iscore/plugins/settingsdelegate/SettingsDelegateFactoryInterface.hpp>

// TODO rename file
class iscore_plugin_pluginsettings :
        public QObject,
        public iscore::Plugin_QtInterface,
        public iscore::SettingsDelegateFactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID SettingsDelegateFactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::Plugin_QtInterface
                iscore::SettingsDelegateFactoryInterface_QtInterface)

    public:
        iscore_plugin_pluginsettings();
        virtual ~iscore_plugin_pluginsettings();

    private:
        iscore::SettingsDelegateFactoryInterface* settings_make() override;

        iscore::Version version() const override;
        UuidKey<iscore::Plugin> key() const override;
};
