#pragma once
#include <iscore/plugins/qt_interfaces/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <QObject>
#include <QStringList>
#include <utility>
#include <vector>

#include <core/application/ApplicationContext.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

namespace iscore {
class Application;
class DocumentDelegateFactoryInterface;
class FactoryListInterface;
class PanelFactory;
}  // namespace iscore

class iscore_plugin_scenario final :
        public QObject,
        public iscore::GUIApplicationContextPlugin_QtInterface,
        public iscore::CommandFactory_QtInterface,
        public iscore::DocumentDelegateFactoryInterface_QtInterface,
        public iscore::PanelFactory_QtInterface,
        public iscore::FactoryList_QtInterface,
        public iscore::FactoryInterface_QtInterface,
        public iscore::PluginRequirementslInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID DocumentDelegateFactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::GUIApplicationContextPlugin_QtInterface
                iscore::CommandFactory_QtInterface
                iscore::DocumentDelegateFactoryInterface_QtInterface
                iscore::PanelFactory_QtInterface
                iscore::FactoryList_QtInterface
                iscore::FactoryInterface_QtInterface
                iscore::PluginRequirementslInterface_QtInterface)

    public:
        iscore_plugin_scenario();

        // Docpanel interface
        std::vector<iscore::DocumentDelegateFactoryInterface*> documents() override;

        // Application plugin interface
        iscore::GUIApplicationContextPlugin* make_applicationPlugin(iscore::Application& app) override;

        // NOTE : implementation is in CommandNames.cpp
        std::pair<const CommandParentFactoryKey, CommandGeneratorMap> make_commands() override;

        std::vector<iscore::PanelFactory*> panels() override;

        // Offre la factory de Process
        std::vector<std::unique_ptr<iscore::FactoryListInterface>> factoryFamilies() override;

        // Crée les objets correspondant aux factories passées en argument.
        // ex. si QString = Process, renvoie un vecteur avec ScenarioFactory.
        std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> factories(
                const iscore::ApplicationContext&,
                const iscore::FactoryBaseKey& factoryName) const override;

        QStringList required() const override;
        QStringList offered() const override;

};
