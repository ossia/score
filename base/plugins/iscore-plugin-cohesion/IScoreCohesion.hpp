#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>

class Dummy {};


/**
 * @brief The IScoreCohesion class
 *
 * This plug-in is here to set-up things that require multiple plug-ins.
 * For instance, if a feature requires the Scenario, the Curve, and the DeviceExplorer,
 * it should certainly be implemented here.
 *
 */
class iscore_plugin_cohesion:
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
        iscore_plugin_cohesion();
        virtual ~iscore_plugin_cohesion() = default;

        virtual iscore::PluginControlInterface* make_control(iscore::Presenter* pres) override;
};

