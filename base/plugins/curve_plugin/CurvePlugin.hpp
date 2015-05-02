#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>

class CurvePlugin:
    public QObject,
    public iscore::FactoryInterface_QtInterface,
    public iscore::PluginControlInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
            iscore::FactoryInterface_QtInterface
            iscore::PluginControlInterface_QtInterface
        )

    public:
        CurvePlugin();
        virtual ~CurvePlugin() = default;

        // Plugin control interface
        virtual iscore::PluginControlInterface* control() override;

        // Process & inspector
        virtual QVector<iscore::FactoryInterface*> factories(const QString& factoryName) override;
};

