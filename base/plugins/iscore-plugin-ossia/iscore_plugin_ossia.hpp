#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

class iscore_plugin_ossia final :
    public QObject,
    public iscore::PluginControlInterface_QtInterface,
    public iscore::FactoryInterface_QtInterface,
        public iscore::PluginRequirementslInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PluginControlInterface_QtInterface_iid)
        Q_INTERFACES(
            iscore::PluginControlInterface_QtInterface
            iscore::FactoryInterface_QtInterface
                iscore::PluginRequirementslInterface_QtInterface
        )

    public:
        iscore_plugin_ossia();
        virtual ~iscore_plugin_ossia() = default;

        iscore::PluginControlInterface* make_control(
                iscore::Application& app) override;

        // Contains the OSC, MIDI, Minuit factories
        std::vector<iscore::FactoryInterfaceBase*> factories(
                const iscore::ApplicationContext&,
                const iscore::FactoryBaseKey& factoryName) const override;

        QStringList required() const override;
};

