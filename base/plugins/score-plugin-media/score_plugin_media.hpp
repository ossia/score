#pragma once
#include <QObject>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>

class score_plugin_media final:
        public QObject,
        public score::Plugin_QtInterface,
        public score::FactoryInterface_QtInterface,
        public score::ApplicationPlugin_QtInterface,
        public score::FactoryList_QtInterface,
        public score::CommandFactory_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                score::Plugin_QtInterface
                score::FactoryInterface_QtInterface
                score::CommandFactory_QtInterface
                score::FactoryList_QtInterface
                score::ApplicationPlugin_QtInterface
                )
  SCORE_PLUGIN_METADATA(1, "142f926e-b2d9-41ce-aff3-a1dab33d3de2")

    public:
        score_plugin_media();
        ~score_plugin_media();

        std::vector<std::unique_ptr<score::InterfaceBase>> factories(
                const score::ApplicationContext& ctx,
                const score::InterfaceKey& factoryName) const override;

        std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands() override;

        std::vector<std::unique_ptr<score::InterfaceListBase>> factoryFamilies() override;
        score::ApplicationPlugin*
        make_applicationPlugin(const score::ApplicationContext& app) override;

};
