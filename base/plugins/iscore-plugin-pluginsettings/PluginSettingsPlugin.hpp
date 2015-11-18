#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/SettingsDelegateFactoryInterface_QtInterface.hpp>

class iscore_plugin_pluginsettings :
    public QObject,
    public iscore::SettingsDelegateFactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID SettingsDelegateFactoryInterface_QtInterface_iid)
        Q_INTERFACES(iscore::SettingsDelegateFactoryInterface_QtInterface)

    public:
        iscore_plugin_pluginsettings();
        virtual ~iscore_plugin_pluginsettings() = default;

        // Settings interface
        iscore::SettingsDelegateFactoryInterface* settings_make() override;
};
