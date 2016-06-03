#pragma once
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <QObject>

#include <iscore/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

// TODO rename file
class iscore_plugin_pluginsettings :
        public QObject,
        public iscore::Plugin_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID Plugin_QtInterface_iid)
        Q_INTERFACES(
                iscore::Plugin_QtInterface)

    public:
        iscore_plugin_pluginsettings();
        virtual ~iscore_plugin_pluginsettings();

    private:
        iscore::Version version() const override;
        UuidKey<iscore::Plugin> key() const override;
};
