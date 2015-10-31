#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>

class AutomationControl;
class iscore_plugin_automation:
        public QObject,
        public iscore::FactoryInterface_QtInterface,
        public iscore::PluginControlInterface_QtInterface,
        public iscore::CommandFactory_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::FactoryInterface_QtInterface
                iscore::PluginControlInterface_QtInterface
                iscore::CommandFactory_QtInterface
                )

    public:
        iscore_plugin_automation();
        virtual ~iscore_plugin_automation() = default;

        // Plugin control interface
        iscore::PluginControlInterface* make_control(
                iscore::Presenter*) override;

        // Process & inspector
        QVector<iscore::FactoryInterface*> factories(
                const QString& factoryName) override;

        // CommandFactory_QtInterface interface
        std::pair<const std::string, CommandGeneratorMap> make_commands() override;
};
