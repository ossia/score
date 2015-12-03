#pragma once
#include <QObject>
#include <faust/dsp/llvm-dsp.h>

#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>

class iscore_plugin_audio final:
        public QObject,
        public iscore::GUIApplicationContextPlugin_QtInterface,
        public iscore::FactoryInterface_QtInterface,
        public iscore::CommandFactory_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::GUIApplicationContextPlugin_QtInterface
                iscore::FactoryInterface_QtInterface
                iscore::CommandFactory_QtInterface
                )

    public:
        iscore_plugin_audio();
        ~iscore_plugin_audio();

        iscore::GUIApplicationContextPlugin* make_applicationPlugin(
                const iscore::ApplicationContext& app) override;

        // Process & inspector
        std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> factories(
                const iscore::ApplicationContext& ctx,
                const iscore::FactoryBaseKey& factoryName) const override;

        // CommandFactory_QtInterface interface
        std::pair<const CommandParentFactoryKey, CommandGeneratorMap> make_commands() override;
};
