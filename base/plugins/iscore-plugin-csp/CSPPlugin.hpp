#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>

/**
 * @brief The iscore_plugin_csp class
 * In this plugin, we prefer the name time relation for what other call constraints.
 */
class iscore_plugin_csp:
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
        iscore_plugin_csp();
        virtual ~iscore_plugin_csp() = default;

        virtual iscore::PluginControlInterface* make_control(iscore::Presenter* pres) override;

        QVector<iscore::FactoryInterface*> factories(const QString& factoryName) override;
};
