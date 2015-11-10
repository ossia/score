#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>

class iscore_plugin_js final:
        public QObject,
        public iscore::FactoryInterface_QtInterface,
        public iscore::CommandFactory_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::FactoryInterface_QtInterface
                iscore::CommandFactory_QtInterface
                )

    public:
        iscore_plugin_js();
        virtual ~iscore_plugin_js() = default;

        // Process & inspector
        std::vector<iscore::FactoryInterface*> factories(
                const QString& factoryName) override;

        // CommandFactory_QtInterface interface
        std::pair<const std::string, CommandGeneratorMap> make_commands() override;
};
