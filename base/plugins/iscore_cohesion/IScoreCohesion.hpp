#pragma once
#include <QObject>
#include <plugin_interface/plugins/PluginControlInterface_QtInterface.hpp>

class Dummy {};


/**
 * @brief The IScoreCohesion class
 *
 * This plug-in is here to set-up things that require multiple plug-ins.
 * For instance, if a feature requires the Scenario, the Curve, and the DeviceExplorer,
 * it should certainly be implemented here.
 *
 */
class IScoreCohesion:
    public QObject,
    public iscore::PluginControlInterface_QtInterface,
    private Dummy
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PluginControlInterface_QtInterface_iid)
        Q_INTERFACES(
            iscore::PluginControlInterface_QtInterface
        )

    public:
        IScoreCohesion();
        virtual ~IScoreCohesion() = default;

        virtual iscore::PluginControlInterface* control_make() override;
};

