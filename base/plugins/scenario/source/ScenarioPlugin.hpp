#pragma once
#include <QObject>
#include <interface/plugins/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <interface/plugins/PluginControlInterface_QtInterface.hpp>

#include <interface/plugins/FactoryFamily_QtInterface.hpp>
#include <interface/plugins/FactoryInterface_QtInterface.hpp>

class ScenarioControl;
class ScenarioPlugin :
    public QObject,
    public iscore::PluginControlInterface_QtInterface,
    public iscore::DocumentDelegateFactoryInterface_QtInterface,
    public iscore::FactoryFamily_QtInterface,
    public iscore::FactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID DocumentDelegateFactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                     iscore::PluginControlInterface_QtInterface
                     iscore::DocumentDelegateFactoryInterface_QtInterface
                     iscore::FactoryFamily_QtInterface
                     iscore::FactoryInterface_QtInterface)

    public:
        ScenarioPlugin();
        virtual ~ScenarioPlugin() = default;

        // Docpanel interface
        virtual QStringList document_list() const override;
        virtual iscore::DocumentDelegateFactoryInterface* document_make(QString name) override;

        // Plugin control interface
        virtual iscore::PluginControlInterface* control_make() override;

        // Offre la factory de Process
        virtual QVector<iscore::FactoryFamily> factoryFamilies_make() override;

        // Crée les objets correspondant aux factories passées en argument.
        // ex. si QString = Process, renvoie un vecteur avec ScenarioFactory.
        virtual QVector<iscore::FactoryInterface*> factories_make(QString factoryName) override;

    private:
        ScenarioControl* m_control;

};
