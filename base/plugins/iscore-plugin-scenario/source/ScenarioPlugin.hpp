#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>

class ScenarioControl;
class iscore_plugin_scenario :
    public QObject,
    public iscore::PluginControlInterface_QtInterface,
    public iscore::DocumentDelegateFactoryInterface_QtInterface,
    public iscore::PanelFactory_QtInterface,
    public iscore::FactoryFamily_QtInterface,
    public iscore::FactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID DocumentDelegateFactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                     iscore::PluginControlInterface_QtInterface
                     iscore::DocumentDelegateFactoryInterface_QtInterface
                     iscore::PanelFactory_QtInterface
                     iscore::FactoryFamily_QtInterface
                     iscore::FactoryInterface_QtInterface)

    public:
        iscore_plugin_scenario();

        // Docpanel interface
        QList<iscore::DocumentDelegateFactoryInterface*> documents() override;

        // Plugin control interface
        iscore::PluginControlInterface* make_control(iscore::Presenter* pres) override;

        QList<iscore::PanelFactory*> panels() override;

        // Offre la factory de Process
        QVector<iscore::FactoryFamily> factoryFamilies() override;

        // Crée les objets correspondant aux factories passées en argument.
        // ex. si QString = Process, renvoie un vecteur avec ScenarioFactory.
        QVector<iscore::FactoryInterface*> factories(const QString& factoryName) override;

    private:
        ScenarioControl* m_control{};

};
