#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>

class iscore_plugin_media final:
        public QObject,
        public iscore::Plugin_QtInterface,
        public iscore::FactoryInterface_QtInterface,
        public iscore::CommandFactory_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::Plugin_QtInterface
                iscore::FactoryInterface_QtInterface
                iscore::CommandFactory_QtInterface
                )
  ISCORE_PLUGIN_METADATA(1, "142f926e-b2d9-41ce-aff3-a1dab33d3de2")

    public:
        iscore_plugin_media();
        ~iscore_plugin_media();

        std::vector<std::unique_ptr<iscore::InterfaceBase>> factories(
                const iscore::ApplicationContext& ctx,
                const iscore::InterfaceKey& factoryName) const override;

        std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands() override;

};
