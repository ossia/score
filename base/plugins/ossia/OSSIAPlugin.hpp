#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>

/**
 * @brief The IScoreCohesion class
 *
 * This plug-in is here to set-up things that require multiple plug-ins.
 * For instance, if a feature requires the Scenario, the Curve, and the DeviceExplorer,
 * it should certainly be implemented here.
 *
 */
class OSSIAPlugin:
    public QObject,
    public iscore::PluginControlInterface_QtInterface,
    public iscore::FactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PluginControlInterface_QtInterface_iid)
        Q_INTERFACES(
            iscore::PluginControlInterface_QtInterface
            iscore::FactoryInterface_QtInterface
        )

    public:
        OSSIAPlugin();
        virtual ~OSSIAPlugin() = default;

        virtual iscore::PluginControlInterface* control() override;

        // Contains the OSC, MIDI, Minuit factories
        QVector<iscore::FactoryInterface*> factories(const QString& factoryName) override;
};

