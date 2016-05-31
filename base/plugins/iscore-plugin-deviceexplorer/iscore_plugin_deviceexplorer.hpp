#pragma once
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>

#include <QObject>
#include <utility>
#include <vector>

#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

namespace iscore {

class FactoryListInterface;
class PanelFactory;
}  // namespace iscore

class iscore_plugin_deviceexplorer final :
        public QObject,
        public iscore::Plugin_QtInterface,
        public iscore::FactoryList_QtInterface,
        public iscore::FactoryInterface_QtInterface,
        public iscore::GUIApplicationContextPlugin_QtInterface,
        public iscore::CommandFactory_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::Plugin_QtInterface
                iscore::FactoryList_QtInterface
                iscore::FactoryInterface_QtInterface
                iscore::GUIApplicationContextPlugin_QtInterface
                iscore::CommandFactory_QtInterface)

    public:
        iscore_plugin_deviceexplorer();
        virtual ~iscore_plugin_deviceexplorer();

    private:
        // Factory for protocols
        std::vector<std::unique_ptr<iscore::FactoryListInterface>> factoryFamilies() override;

        std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> factories(
                const iscore::ApplicationContext& ctx,
                const iscore::AbstractFactoryKey& factoryName) const override;

        // application plugin
        iscore::GUIApplicationContextPlugin* make_applicationPlugin(
                const iscore::GUIApplicationContext& app) override;

        std::pair<const CommandParentFactoryKey, CommandGeneratorMap> make_commands() override;

        QStringList offered() const override;
        iscore::Version version() const override;
        UuidKey<iscore::Plugin> key() const override;
};
