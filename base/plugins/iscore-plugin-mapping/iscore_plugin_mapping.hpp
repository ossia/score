#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>

class iscore_plugin_mapping:
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
        iscore_plugin_mapping();
        virtual ~iscore_plugin_mapping() = default;

        // Process & inspector
        std::vector<iscore::FactoryInterfaceBase*> factories(
                const iscore::ApplicationContext& ctx,
                const iscore::FactoryBaseKey& factoryName) const override;

        // CommandFactory_QtInterface interface
        std::pair<const CommandParentFactoryKey, CommandGeneratorMap> make_commands() override;
};
