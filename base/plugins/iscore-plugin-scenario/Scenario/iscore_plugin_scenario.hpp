#pragma once
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <QObject>
#include <QStringList>
#include <utility>
#include <vector>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

namespace iscore {

class DocumentDelegateFactory;
class FactoryListInterface;
class PanelFactory;
}  // namespace iscore

class iscore_plugin_scenario final :
        public QObject,
        public iscore::GUIApplicationContextPlugin_QtInterface,
        public iscore::CommandFactory_QtInterface,
        public iscore::FactoryList_QtInterface,
        public iscore::FactoryInterface_QtInterface,
        public iscore::Plugin_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID GUIApplicationContextPlugin_QtInterface_iid)
        Q_INTERFACES(
                iscore::GUIApplicationContextPlugin_QtInterface
                iscore::CommandFactory_QtInterface
                iscore::FactoryList_QtInterface
                iscore::FactoryInterface_QtInterface
                iscore::Plugin_QtInterface)

    public:
        iscore_plugin_scenario();
        virtual ~iscore_plugin_scenario();

    private:
        // Application plugin interface
        iscore::GUIApplicationContextPlugin* make_applicationPlugin(const iscore::ApplicationContext& app) override;

        // NOTE : implementation is in CommandNames.cpp
        std::pair<const CommandParentFactoryKey, CommandGeneratorMap> make_commands() override;

        // Offre la factory de Process
        std::vector<std::unique_ptr<iscore::FactoryListInterface>> factoryFamilies() override;

        // Crée les objets correspondant aux factories passées en argument.
        // ex. si QString = Process, renvoie un vecteur avec ScenarioFactory.
        std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> factories(
                const iscore::ApplicationContext&,
                const iscore::AbstractFactoryKey& factoryName) const override;

        QStringList required() const override;
        QStringList offered() const override;

        iscore::Version version() const override;
        UuidKey<iscore::Plugin> key() const override;

};
