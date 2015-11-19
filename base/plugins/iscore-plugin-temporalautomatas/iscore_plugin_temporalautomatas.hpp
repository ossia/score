#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>

namespace TemporalAutomatas
{
class ApplicationPlugin;
}
class iscore_plugin_temporalautomatas final:
        public QObject,
        public iscore::PluginControlInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PluginControlInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::PluginControlInterface_QtInterface
                )

    public:
        iscore_plugin_temporalautomatas();
        virtual ~iscore_plugin_temporalautomatas() = default;

        iscore::PluginControlInterface* make_control(
                iscore::Application& app);

};
