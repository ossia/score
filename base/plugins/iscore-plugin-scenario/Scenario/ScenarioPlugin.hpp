#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

class ScenarioControl;
// TODO rename file
class iscore_plugin_scenario final :
        public QObject,
        public iscore::PluginControlInterface_QtInterface,
        public iscore::CommandFactory_QtInterface,
        public iscore::DocumentDelegateFactoryInterface_QtInterface,
        public iscore::PanelFactory_QtInterface,
        public iscore::FactoryFamily_QtInterface,
        public iscore::FactoryInterface_QtInterface,
        public iscore::PluginRequirementslInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID DocumentDelegateFactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::PluginControlInterface_QtInterface
                iscore::CommandFactory_QtInterface
                iscore::DocumentDelegateFactoryInterface_QtInterface
                iscore::PanelFactory_QtInterface
                iscore::FactoryFamily_QtInterface
                iscore::FactoryInterface_QtInterface
                iscore::PluginRequirementslInterface_QtInterface)

    public:
        iscore_plugin_scenario();

        // Docpanel interface
        QList<iscore::DocumentDelegateFactoryInterface*> documents() override;

        // Plugin control interface
        iscore::PluginControlInterface* make_control(iscore::Application& app) override;

        // NOTE : implementation is in CommandNames.cpp
        std::pair<const CommandParentFactoryKey, CommandGeneratorMap> make_commands() override;

        QList<iscore::PanelFactory*> panels() override;

        // Offre la factory de Process
        std::vector<iscore::FactoryFamily> factoryFamilies() override;

        // Crée les objets correspondant aux factories passées en argument.
        // ex. si QString = Process, renvoie un vecteur avec ScenarioFactory.
        std::vector<iscore::FactoryInterfaceBase*> factories(
                const iscore::FactoryBaseKey& factoryName) const override;

        QStringList required() const override;
        QStringList offered() const override;

};
